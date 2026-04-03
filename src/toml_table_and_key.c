#include <ctype.h>
#include <limits.h>
#include <xcb/xproto.h>
#include <xkbcommon/xkbcommon.h>

/**
 * This handles all the parsing of tables and keys.  All tables and their
 * corresponding keys are defined in `table_definitions`.
 * Directly below here are declarations of utility functions and the specific
 * key parsing function.  There is also the append functions for table arrays.
 * Each key parsing function is called with no valued read yet, each has its own
 * specific parsing while still using valid TOML values.
 */

#include "toml.h"

/* append functions for table arrays */
static void append_wm_monitor(struct toml_parse_context *context);
static void append_wm_workspace(struct toml_parse_context *context);
static void append_wm_window(struct toml_parse_context *context);
static void append_wm_binding(struct toml_parse_context *context);
static void append_wm_startup(struct toml_parse_context *context);

/* utility functions for common properties */
static void parse_border_size(struct toml_parse_context *context, struct wm_border *border);
static void parse_border_radius(struct toml_parse_context *context, struct wm_border *border);
static void parse_border_color(struct toml_parse_context *context, struct wm_border *border);
static void parse_layout(struct toml_parse_context *context, enum tiling_layout *layout);
static enum window_mode resolve_window_mode(struct toml_parse_context *context, const char *string);
/* Try to resolve the given string to a modifier constant. */
static uint16_t resolve_modifier(struct toml_parse_context *context, const char *modifier);
/* Translate the given string to a button index. */
static xcb_button_t resolve_button(struct toml_parse_context *context, const char *name);
static enum action_type resolve_action(struct toml_parse_context *context, const char *string);
static union action_value resolve_action_value( struct toml_parse_context *context, const char *string);
static void parse_action(struct toml_parse_context *context, enum action_type *type);
static void parse_action_value(struct toml_parse_context *context, union action_value *value);

/* parse functions for specific keys */
static void parse_wm_gaps(struct toml_parse_context *context);
static void parse_wm_border_size(struct toml_parse_context *context);
static void parse_wm_border_decoration(struct toml_parse_context *context);
static void parse_wm_border_radius(struct toml_parse_context *context);
static void parse_wm_border_color(struct toml_parse_context *context);
static void parse_wm_monitor_name(struct toml_parse_context *context);
static void parse_wm_monitor_layout(struct toml_parse_context *context);
static void parse_wm_workspace_name(struct toml_parse_context *context);
static void parse_wm_workspace_number(struct toml_parse_context *context);
static void parse_wm_workspace_monitor(struct toml_parse_context *context);
static void parse_wm_workspace_layout(struct toml_parse_context *context);
static void parse_wm_window_name(struct toml_parse_context *context);
static void parse_wm_window_class(struct toml_parse_context *context);
static void parse_wm_window_instance(struct toml_parse_context *context);
static void parse_wm_window_workspace(struct toml_parse_context *context);
static void parse_wm_window_monitor(struct toml_parse_context *context);
static void parse_wm_window_hidden(struct toml_parse_context *context);
static void parse_wm_window_mode(struct toml_parse_context *context);
static void parse_wm_window_border_size(struct toml_parse_context *context);
static void parse_wm_window_border_decoration(struct toml_parse_context *context);
static void parse_wm_window_border_radius(struct toml_parse_context *context);
static void parse_wm_window_border_color(struct toml_parse_context *context);
static void parse_wm_binding_release(struct toml_parse_context *context);
static void parse_wm_binding_transparent(struct toml_parse_context *context);
static void parse_wm_binding_modifiers(struct toml_parse_context *context);
static void parse_wm_binding_button(struct toml_parse_context *context);
static void parse_wm_binding_key(struct toml_parse_context *context);
static void parse_wm_binding_key_code(struct toml_parse_context *context);
static void parse_wm_binding_action(struct toml_parse_context *context);
static void parse_wm_binding_value(struct toml_parse_context *context);
static void parse_wm_bindings(struct toml_parse_context *context);
static void parse_wm_startup_action(struct toml_parse_context *context);
static void parse_wm_startup_value(struct toml_parse_context *context);

/* definition of all tables and their sub table relation */
static const struct {
    /* if this is incremented, it becomes a sub table of the previous entry */
    int depth;
    /* name of the table/key or * for any key */
    const char *name;
    /* function which parses the value */
    void (*parser)(struct toml_parse_context *context);
    /* function that adds a new array entry */
    void (*appender)(struct toml_parse_context *context);
} table_definitions[] = {
    /* dummy entry so that `table + 1` (by default `0 + 1`) evaluates to the
     * `wm` entry, more entries with depth 1 might be added in the future, for
     * example for a bar or compositor settings
     */
    { 0, "dummy-root", NULL, NULL },
    { 1, "wm", NULL, NULL },
        { 2, "gaps", NULL, NULL },
            { 3, "inner", parse_wm_gaps, NULL },
            { 3, "outer", parse_wm_gaps, NULL },
        { 2, "border", NULL, NULL },
            { 3, "size", parse_wm_border_size, NULL },
            { 3, "decoration", parse_wm_border_decoration, NULL },
            { 3, "radius", NULL, NULL },
                { 4, "inner", parse_wm_border_radius, NULL },
                { 4, "outer", parse_wm_border_radius, NULL },
            { 3, "color", NULL, NULL },
                { 4, "focused", parse_wm_border_color, NULL },
                { 4, "highlight", parse_wm_border_color, NULL },
                { 4, "inactive", parse_wm_border_color, NULL },
                { 4, "floating", parse_wm_border_color, NULL },
                { 4, "tiling", parse_wm_border_color, NULL },
        { 2, "monitor", NULL, append_wm_monitor },
            { 3, "name", parse_wm_monitor_name, NULL },
            { 3, "layout", parse_wm_monitor_layout, NULL },
        { 2, "workspace", NULL, append_wm_workspace },
            { 3, "name", parse_wm_workspace_name, NULL },
            { 3, "number", parse_wm_workspace_number, NULL },
            { 3, "monitor", parse_wm_workspace_monitor, NULL },
            { 3, "layout", parse_wm_workspace_layout, NULL },
        { 2, "window", NULL, append_wm_window },
            { 3, "name", parse_wm_window_name, NULL },
            { 3, "title", parse_wm_window_name, NULL },
            { 3, "class", parse_wm_window_class, NULL },
            { 3, "instance", parse_wm_window_instance, NULL },
            { 3, "workspace", parse_wm_window_workspace, NULL },
            { 3, "monitor", parse_wm_window_monitor, NULL },
            { 3, "hidden", parse_wm_window_hidden, NULL },
            { 3, "mode", parse_wm_window_mode, NULL },
            { 3, "border", NULL, NULL },
                { 4, "size", parse_wm_window_border_size, NULL },
                { 4, "decoration", parse_wm_window_border_decoration, NULL },
                { 4, "radius", NULL, NULL },
                    { 5, "inner", parse_wm_window_border_radius, NULL },
                    { 5, "outer", parse_wm_window_border_radius, NULL },
                { 4, "color", NULL, NULL },
                    { 5, "focused", parse_wm_window_border_color, NULL },
                    { 5, "highlight", parse_wm_window_border_color, NULL },
                    { 5, "inactive", parse_wm_window_border_color, NULL },
                    { 5, "floating", parse_wm_window_border_color, NULL },
                    { 5, "tiling", parse_wm_window_border_color, NULL },
        { 2, "binding", NULL, append_wm_binding },
            { 3, "release", parse_wm_binding_release, NULL },
            { 3, "transparent", parse_wm_binding_transparent, NULL },
            { 3, "modifiers", parse_wm_binding_modifiers, NULL },
            { 3, "button", parse_wm_binding_button, NULL },
            { 3, "key", parse_wm_binding_key, NULL },
            { 3, "key-code", parse_wm_binding_key_code, NULL },
            { 3, "action", parse_wm_binding_action, NULL },
            { 3, "argument", parse_wm_binding_value, NULL },
        { 2, "bindings", NULL, NULL },
            { 3, "*", parse_wm_bindings, NULL },
        { 2, "startup", NULL, append_wm_startup },
            { 3, "action", parse_wm_startup_action, NULL },
            { 3, "argument", parse_wm_startup_value, NULL },
};

/* Get a table index of the table name @name that is a sub table of @table. */
static unsigned get_sub_table(struct toml_parse_context *context,
        unsigned table, const char *name)
{
    const char *other_name;
    int depth, other_depth;
    unsigned index;

    /* get the depth + 1 and find each entry with this depth without moving out
     * and while skipping deeper tables
     */
    depth = table_definitions[table].depth + 1;
    for (index = table + 1; index < SIZE(table_definitions); index++) {
        other_depth = table_definitions[index].depth;
        /* skip deeper tables */
        if (depth < other_depth) {
            continue;
        }
        /* stop if we move out of the parent table */
        if (depth > other_depth) {
            index = SIZE(table_definitions);
            break;
        }

        other_name = table_definitions[index].name;
        if (other_name[0] == '*' || strcmp(name, other_name) == 0) {
            break;
        }
    }

    if (index == SIZE(table_definitions)) {
        emit_error(context, "table %s does not exist", name);
    }

    return index;
}

static void append_wm_monitor(struct toml_parse_context *context)
{
    LIST_APPEND(context->wm.monitor, NULL, 1);
}

static void append_wm_workspace(struct toml_parse_context *context)
{
    LIST_APPEND(context->wm.workspace, NULL, 1);
}

static void append_wm_window(struct toml_parse_context *context)
{
    LIST_APPEND(context->wm.window, NULL, 1);
}

static void append_wm_binding(struct toml_parse_context *context)
{
    LIST_APPEND(context->wm.binding, NULL, 1);
}

static void append_wm_startup(struct toml_parse_context *context)
{
    LIST_APPEND(context->wm.startup, NULL, 1);
}

static void parse_border_size(struct toml_parse_context *context,
        struct wm_border *border)
{
    read_integer(context);
    /* TODO: bounds check */
    border->size = context->number;
}

static void parse_border_decoration(struct toml_parse_context *context,
        struct wm_border *border)
{
    const char *modes[] = {
        [BORDER_NONE] = "none",
        [BORDER_SIMPLE] = "simple",
        [BORDER_FULL] = "full"
    };

    enum border_decoration decoration;

    read_any_string(context);

    switch (context->string[0]) {
    case 'n': decoration = BORDER_NONE; break;
    case 's': decoration = BORDER_SIMPLE; break;
    case 'f': decoration = BORDER_FULL; break;
    default: decoration = 0; break;
    }

    if (strcmp(modes[decoration], context->string) != 0) {
        emit_error(context, "invalid decoration constant, choose one of: "
                "none, simple, full");
    }

    border->decoration = decoration;
}

static void parse_border_radius(
        struct toml_parse_context *context,
        struct wm_border *border)
{
    read_integer(context);
    /* TODO: bounds check */
    if (context->string[0] == 'i') {
        border->radius.inner = context->number;
    } else {
        border->radius.outer = context->number;
    }
}

static void parse_border_color(struct toml_parse_context *context,
        struct wm_border *border)
{
    xcb_render_color_t *pointer;

    switch (context->string[1]) {
    case 'o': pointer = &border->color.focused; break;
    case 'c': pointer = &border->color.highlight; break;
    case 'n': pointer = &border->color.inactive; break;
    case 'l': pointer = &border->color.floating; break;
    case 'i': pointer = &border->color.tiling; break;
        break;
    }
    read_any_string(context);
    /* TODO: parse color */
}

static void parse_layout(struct toml_parse_context *context,
        enum tiling_layout *layout_pointer)
{
    const char *layouts[] = {
        [TILE_UNSPECIFIED] = "unspecified",
        [TILE_MANUAL] = "manual",
        [TILE_HORIZONTAL] = "horizontal",
        [TILE_VERTICAL] = "vertical",
        [TILE_GRID] = "grid",
        [TILE_SPIRAL] = "spiral"
    };

    enum tiling_layout layout;

    read_any_string(context);

    switch (context->string[0]) {
    case 'u': layout = TILE_UNSPECIFIED; break;
    case 'm': layout = TILE_MANUAL; break;
    case 'h': layout = TILE_HORIZONTAL; break;
    case 'v': layout = TILE_VERTICAL; break;
    case 'g': layout = TILE_GRID; break;
    case 's': layout = TILE_SPIRAL; break;
    default: layout = 0; break;
    }

    if (strcmp(layouts[layout], context->string) != 0) {
        emit_error(context, "invalid layout constant, choose one of: "
                "unspecified, manual, horizontal, vertical, grid, spiral");
    }

    *layout_pointer = layout;
}

static enum window_mode resolve_window_mode(struct toml_parse_context *context,
        const char *string)
{
    const char *modes[] = {
        [WINDOW_UNSPECIFIED] = "unspecified",
        [WINDOW_TILING] = "tiling",
        [WINDOW_FLOATING] = "floating",
        [WINDOW_FULLSCREEN] = "fullscreen",
    };

    enum window_mode mode;

    if (context->string[0] == '\0') {
        mode = 0;
    } else {
        switch (context->string[1]) {
        case 'n': mode = WINDOW_UNSPECIFIED; break;
        case 'i': mode = WINDOW_TILING; break;
        case 'l': mode = WINDOW_FLOATING; break;
        case 'u': mode = WINDOW_FULLSCREEN; break;
        default: mode = 0; break;
        }
    }

    if (strcmp(modes[mode], context->string) != 0) {
        emit_error(context, "invalid mode constant, choose one of: "
                "unspecified, tiling, floating, fullscreen");
    }

    return mode;
}

/* Try to resolve the given string within parser to a modifier constant. */
static uint16_t resolve_modifier(struct toml_parse_context *context,
        const char *modifier)
{
    /* string to modifier translation */
    static const struct {
        const char *string;
        uint16_t modifier;
    } string_to_modifier[] = {
        { "None", 0 },
        { "Shift", XCB_MOD_MASK_SHIFT },
        { "Lock", XCB_MOD_MASK_LOCK },
        { "Control", XCB_MOD_MASK_CONTROL },
        { "Mod1", XCB_MOD_MASK_1 },
        { "Mod2", XCB_MOD_MASK_2 },
        { "Mod3", XCB_MOD_MASK_3 },
        { "Mod4", XCB_MOD_MASK_4 },
        { "Mod5", XCB_MOD_MASK_5 },
    };

    unsigned index;

    if (strlen(modifier) < 4) {
        emit_error(context, "invalid modifier");
    }

    /* the fourth character is unique among all constants */
    switch (modifier[3]) {
    case 'e': index = 0; break;
    case 'f': index = 1; break;
    case 'k': index = 2; break;
    case 't': index = 3; break;
    case '1': index = 4; break;
    case '2': index = 5; break;
    case '3': index = 6; break;
    case '4': index = 7; break;
    case '5': index = 8; break;
    default:
        emit_error(context, "invalid modifier");
    }

    if (strcmp(string_to_modifier[index].string, modifier) != 0) {
        emit_error(context, "invalid modifier");
    }

    return string_to_modifier[index].modifier;
}

/* Translate the string within @parser to a button index.
 *
 * @return BUTTON_NONE when the string is not a button constant.
 */
static xcb_button_t resolve_button(struct toml_parse_context *context,
        const char *name)
{
    /* conversion from string to button index */
    static const struct button_string {
        /* the string representation of the button */
        const char *name;
        /* the button index */
        xcb_button_t button_index;
    } button_strings[] = {
        /* buttons can also be Button[1-9][0-9]? or X[1-9] to directly address
         * the index
         */
        { "LButton", 1 },
        { "Left", 1 },
        { "LeftButton", 1 },

        { "MButton", 2 },
        { "Middle", 2 },
        { "MiddleButton", 2 },

        { "RButton", 3 },
        { "Right", 3 },
        { "RightButton", 3 },

        { "ScrollUp", 4 },
        { "WheelUp", 4 },

        { "ScrollDown", 5 },
        { "WheelDown", 5 },

        { "ScrollLeft", 6 },
        { "WheelLeft", 6 },

        { "ScrollRight", 7 },
        { "WheelRight", 7 },
    };

    xcb_button_t index = 0;

    /* parse strings starting with "X" */
    if (name[0] == 'X') {
        name += strlen("X");

        if (name[0] < '1' || name[0] > '9') {
            emit_error(context, "expected digit after 'X'");
        }
        index = name[0] - '1';
        name++;

        /* make sure X1 comes right after WheelRight */
        index += 8;

        if (name[0] != '\0') {
            index = 0;
        }
    /* parse strings starting with "Button" */
    } else if (strncmp(name, "Button", strlen("Button")) == 0) {
        name += strlen("Button");

        if (name[0] < '0' || name[0] > '9') {
            emit_error(context, "expected digit after 'Button'");
        }
        index = name[0] - '0';
        name++;

        if (name[0] >= '0' && name[0] <= '9') {
            index *= 10;
            index += name[0] - '0';
            name++;
        }

        if (name[0] != '\0') {
            index = 0;
        } else {
            /* represent it as 1-based internally */
            index++;
        }
    } else {
        for (unsigned i = 0; i < SIZE(button_strings); i++) {
            if (strcmp(name, button_strings[i].name) == 0) {
                index = button_strings[i].button_index;
                break;
            }
        }
    }

    if (index == 0) {
        emit_error(context, "invalid button name");
    }

    return index;
}

static enum action_type resolve_action(struct toml_parse_context *context,
        const char *string)
{
    /* TODO: implement when actions are there */
    return ACTION_NONE;
}

static union action_value resolve_action_value(
        struct toml_parse_context *context,
        const char *string)
{
    union action_value value;

    /* TODO: implement when actions are there */
    value.value = 0;

    return value;
}

static void parse_action(struct toml_parse_context *context,
        enum action_type *type)
{
    read_any_string(context);
    *type = resolve_action(context, context->string);
}

static void parse_action_value(struct toml_parse_context *context,
        union action_value *value)
{
    read_any_string(context);
    *value = resolve_action_value(context, context->string);
}

static void parse_wm_gaps(struct toml_parse_context *context)
{
    int *gaps;
    int character;
    unsigned index;

    if (context->string[0] == 'i') {
        gaps = context->wm.gaps.inner;
    } else {
        gaps = context->wm.gaps.outer;
    }

    /* read the first character of the array '[' or a digit indicating the gaps
     * are defined through a single number
     */
    character = skip_space(context, false);
    if (character == '[') {
        character = skip_space(context, true);
        ungetc(character, context->file);

        /* read up to 4 integers separated by a comma */
        for (index = 0; index < 4; index++) {
            read_integer(context);
            /* TODO: bounds check */
            gaps[index] = context->number;
            character = skip_space(context, true);
            if (character != ',') {
                break;
            }

            /* allow for trailing , */
            character = skip_space(context, true);
            if (character == ']') {
                break;
            }

            ungetc(character, context->file);
        }

        if (index > 3) {
            emit_error(context, "too many integers, need a maximum of 4");
        } else if (index != 1 && index != 3) {
            emit_error(context,
                    "expected ',' and another integer for 2 or 4 in total");
        }

        if (character != ']') {
            emit_error(context, "expected ']' after 2/4 integers");
        }

        /* copy the left/top gaps to the right/bottom gaps */
        if (index == 1) {
            gaps[2] = gaps[0];
            gaps[3] = gaps[1];
        }
    } else if (isdigit(character)) {
        ungetc(character, context->file);
        read_integer(context);
        /* make all gaps the same */
        for (index = 0; index < 4; index++) {
            gaps[index] = context->number;
        }
    } else {
        emit_error(context, "expected g, [ h, v ] or [ l, t, r, b ]");
    }
}

static void parse_wm_border_size(struct toml_parse_context *context)
{
    parse_border_size(context, &context->wm.border);
}

static void parse_wm_border_decoration(struct toml_parse_context *context)
{
    parse_border_decoration(context, &context->wm.border);
}

static void parse_wm_border_radius(
        struct toml_parse_context *context)
{
    parse_border_radius(context, &context->wm.border);
}

static void parse_wm_border_color(struct toml_parse_context *context)
{
    parse_border_color(context, &context->wm.border);
}

static void parse_wm_monitor_name(struct toml_parse_context *context)
{
    if (context->wm.monitor_length == 0) {
        emit_error(context,
                "can not modify 'monitor' if no entry was defined yet");
    }

    read_any_string(context);
    context->wm.monitor[context->wm.monitor_length - 1].name =
        xstrdup(context->string);
}

static void parse_wm_monitor_layout(struct toml_parse_context *context)
{
    if (context->wm.monitor_length == 0) {
        emit_error(context,
                "can not modify 'monitor' if no entry was defined yet");
    }

    parse_layout(context,
            &context->wm.monitor[context->wm.monitor_length - 1].layout);
}

static void parse_wm_workspace_name(struct toml_parse_context *context)
{
    if (context->wm.workspace_length == 0) {
        emit_error(context,
                "can not modify 'workspace' if no entry was defined yet");
    }

    read_any_string(context);
    context->wm.workspace[context->wm.workspace_length - 1].name =
        xstrdup(context->string);
}

static void parse_wm_workspace_number(struct toml_parse_context *context)
{
    if (context->wm.workspace_length == 0) {
        emit_error(context,
                "can not modify 'workspace' if no entry was defined yet");
    }

    read_integer(context);
    /* TODO: bounds check */
    context->wm.workspace[context->wm.workspace_length - 1].number =
        context->number;
}

static void parse_wm_workspace_monitor(struct toml_parse_context *context)
{
    if (context->wm.workspace_length == 0) {
        emit_error(context,
                "can not modify 'workspace' if no entry was defined yet");
    }

    read_any_string(context);
    context->wm.workspace[context->wm.workspace_length - 1].monitor =
        xstrdup(context->string);
}

static void parse_wm_workspace_layout(struct toml_parse_context *context)
{
    if (context->wm.workspace_length == 0) {
        emit_error(context,
                "can not modify 'workspace' if no entry was defined yet");
    }

    parse_layout(context,
            &context->wm.workspace[context->wm.workspace_length - 1].layout);
}

static void parse_wm_window_name(struct toml_parse_context *context)
{
    if (context->wm.window_length == 0) {
        emit_error(context,
                "can not modify 'window' if no entry was defined yet");
    }

    read_any_string(context);
    context->wm.window[context->wm.window_length - 1].name =
        xstrdup(context->string);
}

static void parse_wm_window_class(struct toml_parse_context *context)
{
    if (context->wm.window_length == 0) {
        emit_error(context,
                "can not modify 'window' if no entry was defined yet");
    }

    read_any_string(context);
    context->wm.window[context->wm.window_length - 1].class =
        xstrdup(context->string);
}

static void parse_wm_window_instance(struct toml_parse_context *context)
{
    if (context->wm.window_length == 0) {
        emit_error(context,
                "can not modify 'window' if no entry was defined yet");
    }

    read_any_string(context);
    context->wm.window[context->wm.window_length - 1].instance =
        xstrdup(context->string);
}

static void parse_wm_window_workspace(struct toml_parse_context *context)
{
    if (context->wm.window_length == 0) {
        emit_error(context,
                "can not modify 'window' if no entry was defined yet");
    }

    read_any_string(context);
    context->wm.window[context->wm.window_length - 1].workspace =
        xstrdup(context->string);
}

static void parse_wm_window_monitor(struct toml_parse_context *context)
{
    if (context->wm.window_length == 0) {
        emit_error(context,
                "can not modify 'window' if no entry was defined yet");
    }

    read_any_string(context);
    context->wm.window[context->wm.window_length - 1].monitor =
        xstrdup(context->string);
}

static void parse_wm_window_hidden(struct toml_parse_context *context)
{
    if (context->wm.window_length == 0) {
        emit_error(context,
                "can not modify 'window' if no entry was defined yet");
    }

    read_boolean(context);
    context->wm.window[context->wm.window_length - 1].hidden = context->number;
}

static void parse_wm_window_mode(struct toml_parse_context *context)
{
    enum window_mode mode;

    if (context->wm.window_length == 0) {
        emit_error(context,
                "can not modify 'window' if no entry was defined yet");
    }

    read_any_string(context);
    mode = resolve_window_mode(context, context->string);
    context->wm.window[context->wm.window_length - 1].mode = mode;
}

static void parse_wm_window_border_size(struct toml_parse_context *context)
{
    if (context->wm.window_length == 0) {
        emit_error(context,
                "can not modify 'window' if no entry was defined yet");
    }

    parse_border_size(context,
            &context->wm.window[context->wm.window_length - 1].border);
}

static void parse_wm_window_border_decoration(struct toml_parse_context *context)
{
    if (context->wm.window_length == 0) {
        emit_error(context,
                "can not modify 'window' if no entry was defined yet");
    }

    parse_border_decoration(context,
            &context->wm.window[context->wm.window_length - 1].border);
}

static void parse_wm_window_border_radius(
        struct toml_parse_context *context)
{
    if (context->wm.window_length == 0) {
        emit_error(context,
                "can not modify 'window' if no entry was defined yet");
    }

    parse_border_radius(context,
            &context->wm.window[context->wm.window_length - 1].border);
}

static void parse_wm_window_border_color(struct toml_parse_context *context)
{
    if (context->wm.window_length == 0) {
        emit_error(context,
                "can not modify 'window' if no entry was defined yet");
    }

    parse_border_color(context,
            &context->wm.window[context->wm.window_length - 1].border);
}

static void parse_wm_binding_transparent(struct toml_parse_context *context)
{
    if (context->wm.binding_length == 0) {
        emit_error(context,
                "can not modify 'binding' if no entry was defined yet");
    }

    read_boolean(context);
    context->wm.binding[context->wm.binding_length - 1].is_transparent =
        context->number;
}

static void parse_wm_binding_release(struct toml_parse_context *context)
{
    if (context->wm.binding_length == 0) {
        emit_error(context,
                "can not modify 'binding' if no entry was defined yet");
    }

    read_boolean(context);
    context->wm.binding[context->wm.binding_length - 1].is_release =
        context->number;
}

static void parse_wm_binding_modifiers(struct toml_parse_context *context)
{
    char *name, *plus;
    uint16_t modifiers = 0;

    if (context->wm.binding_length == 0) {
        emit_error(context,
                "can not modify 'binding' if no entry was defined yet");
    }

    read_any_string(context);

    /* parse the modifiers separated by '+' */
    name = context->string;
    do  {
        plus = strchr(name, '+');
        if (plus != NULL) {
            plus[0] = '\0';
        }
        modifiers |= resolve_modifier(context, name);
        name = plus + 1;
    } while (plus != NULL);

    context->wm.binding[context->wm.binding_length - 1].modifiers =
        modifiers;
}

static void parse_wm_binding_button(struct toml_parse_context *context)
{
    xcb_button_t button;

    if (context->wm.binding_length == 0) {
        emit_error(context,
                "can not modify 'binding' if no entry was defined yet");
    }

    read_any_string(context);
    button = resolve_button(context, context->string);
    context->wm.binding[context->wm.binding_length - 1].button =
        button;
}

static void parse_wm_binding_key(struct toml_parse_context *context)
{
    xkb_keysym_t key_symbol;

    if (context->wm.binding_length == 0) {
        emit_error(context,
                "can not modify 'binding' if no entry was defined yet");
    }

    read_any_string(context);
    key_symbol = xkb_keysym_from_name(context->string, XKB_KEYSYM_NO_FLAGS);
    if (key_symbol == XKB_KEY_NoSymbol) {
        printf("invalid key symbol: %s\n", context->string);
    } else {
        context->wm.binding[context->wm.binding_length - 1].key_symbol =
            key_symbol;
    }
}

static void parse_wm_binding_key_code(struct toml_parse_context *context)
{
    if (context->wm.binding_length == 0) {
        emit_error(context,
                "can not modify 'binding' if no entry was defined yet");
    }

    read_integer(context);
    if (!xkb_keycode_is_legal_x11(context->number)) {
        emit_error(context,
                "%ld is not a valid x11 keycode\n", context->number);
    } else {
        context->wm.binding[context->wm.binding_length - 1].key_code =
            context->number;
    }
}

static void parse_wm_binding_action(struct toml_parse_context *context)
{
    if (context->wm.binding_length == 0) {
        emit_error(context,
                "can not modify 'binding' if no entry was defined yet");
    }

    parse_action(context,
            &context->wm.binding[context->wm.binding_length - 1].action);
}

static void parse_wm_binding_value(struct toml_parse_context *context)
{
    if (context->wm.binding_length == 0) {
        emit_error(context,
                "can not modify 'binding' if no entry was defined yet");
    }

    parse_action_value(context,
            &context->wm.binding[context->wm.binding_length - 1].value);
}

/* Parse keys within a [wm.bindings] table. */
static void parse_wm_bindings(struct toml_parse_context *context)
{
    char *key_name, *plus, *colon;
    struct wm_binding binding;

    ZERO(&binding, 1);

    /* read words separated by '+', leave the last word for the outside */
    key_name = context->string;
    while (plus = strchr(key_name, '+'), plus != NULL) {
        plus[0] = '\0';
        binding.modifiers |= resolve_modifier(context, key_name);
        key_name = plus + 1;
    }

    /* interpret the last words as key symbol or button */
    binding.key_symbol = xkb_keysym_from_name(key_name, XKB_KEYSYM_NO_FLAGS);
    if (binding.key_symbol == XKB_KEY_NoSymbol) {
        binding.button = resolve_button(context, key_name);
    }

    read_any_string(context);

    /* parse a "action:argument" pair, the ":argument" part is optional */
    colon = strchr(context->string, ':');
    if (colon != NULL) {
        colon[0] = '\0';
        binding.value = resolve_action_value(context, colon + 1);
    }

    binding.action = resolve_action(context, context->string);

    LIST_APPEND_VALUE(context->wm.binding, binding);
}

static void parse_wm_startup_action(struct toml_parse_context *context)
{
    if (context->wm.startup_length == 0) {
        emit_error(context,
                "can not modify 'startup' if no entry was defined yet");
    }

    parse_action(context,
            &context->wm.startup[context->wm.startup_length - 1].action);
}

static void parse_wm_startup_value(struct toml_parse_context *context)
{
    if (context->wm.startup_length == 0) {
        emit_error(context,
                "can not modify 'startup' if no entry was defined yet");
    }

    parse_action_value(context,
            &context->wm.startup[context->wm.startup_length - 1].value);
}

/* Parse a table header [[?word(.word)*]?] and move into the table. */
void parse_table_header(struct toml_parse_context *context)
{
    unsigned table = 0;
    int character;
    bool has_double_brackets = false;

    character = fgetc(context->file);
    if (character == '[') {
        has_double_brackets = true;
    } else {
        ungetc(character, context->file);
    }

    /* read words separated by '.' */
    do {
        read_word(context);
        table = get_sub_table(context, table, context->string);
    } while (character = skip_space(context, false), character == '.');

    /* check that the right bracketing is used */
    if (table_definitions[table].appender != NULL) {
        if (!has_double_brackets) {
            emit_error(context, "this is a table array, use [[...]]");
        }
        (*table_definitions[table].appender)(context);
    } else {
        if (has_double_brackets) {
            emit_error(context, "this is not a table array, use [...]");
        }
    }

    context->table = table;

    /* for example "[wm.border.size]" is used but this requires a value and can
     * not be a table header
     */
    if (table_definitions[table].parser != NULL) {
        emit_error(context, "key can not be used as table");
    }

    /* check for ] and ]] endings */
    if (character != ']') {
        emit_error(context, "expected '%s' to finish table header",
                has_double_brackets ? "]]" : "]");
    } else if (has_double_brackets) {
        character = fgetc(context->file);
        if (character != ']') {
            emit_error(context, "starting '[[' requires ending ']]'");
        }
    }

    character = skip_space(context, false);
    if (character != '\n' && character != EOF) {
        emit_error(context, "expected line end after table header");
    }
}

/* Parse a list of tables separated by , defined by { keys... }. */
static void parse_table_array(struct toml_parse_context *context)
{
    int character;

    /* read characters until the array ending ']' */
    while (character = skip_space(context, true), character != ']') {
        if (character != '{') {
            emit_error(context, "expected '{' for inline table");
        }
        /* append an empty element */
        (*table_definitions[context->table].appender)(context);
        parse_table(context, true);

        character = skip_space(context, true);
        if (character != ',') {
            break;
        }

        /* allow for trailing , */
        character = skip_space(context, true);
        if (character == ']') {
            break;
        }

        ungetc(character, context->file);
    }

    if (character != ']') {
        emit_error(context, "expected ']' to end table array");
    }
}

/* Parse the content of a table. */
void parse_table(struct toml_parse_context *context, bool is_inline)
{
    int character;
    unsigned table, old_table;

    /* read characters until '}' is reached for inline tables or the next table
     * header '[' is reached for non inline tables
     */
    while (character = skip_space(context, true),
            character != EOF &&
            ((is_inline && character != '}') ||
                (!is_inline && character != '['))) {

        table = context->table;

        /* read a series of words separated by '.' and find the entries in the
         * `table_definitions`
         */
        ungetc(character, context->file);
        do {
            read_word(context);
            table = get_sub_table(context, table, context->string);
        } while (character = skip_space(context, false), character == '.');

        if (character != '=') {
            emit_error(context, "expected '=' after key name");
        }

        /* parse the value of the key */
        if (table_definitions[table].parser != NULL) {
            (*table_definitions[table].parser)(context);
        } else if (table_definitions[table].appender != NULL) {
            character = skip_space(context, false);
            if (character != '[') {
                emit_error(context, "expected '[' after '=' for table array");
            }
            old_table = context->table;
            context->table = table;
            parse_table_array(context);
            context->table = old_table;
        } else {
            character = skip_space(context, false);
            if (character != '{') {
                emit_error(context, "expected '{' after '=' for inline table");
            }
            old_table = context->table;
            context->table = table;
            parse_table(context, true);
            context->table = old_table;
        }

        /* check for a comma for inline tables */
        if (is_inline) {
            character = skip_space(context, true);
            if (character == '}') {
                break;
            }
            if (character != ',') {
                emit_error(context, "expected ',' or '}' after key/value pair");
            }
        } else {
            character = skip_space(context, false);
            if (character != '\n' && character != EOF) {
                emit_error(context, "expected line end after key/value pair");
            }
        }
    }

    if (character == EOF && is_inline) {
        emit_error(context, "expected closing '}' before EOF");
    }

    if (!is_inline) {
        ungetc(character, context->file);
    }
}
