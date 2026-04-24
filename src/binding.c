#include <xcb/xproto.h>

#include <utility/utility.h>

/**
 * Bindings are stored in an array that associates a key code/modifiers with a
 * list of press and release actions.
 *
 * The binding arrays are optimized for fast lookups.
 */

#include "binding.h"
#include "x11.h"

/* if the binding should pass through to the underlying window */
#define BINDING_TRANSPARENT 0x1

/* a binding consists of a list of actions for pressing and releasing */
struct binding {
    /* an OR combination of the above `BINDING_*` flags */
    unsigned flags;
    /* the actions to execute when the button/key is pressed (terminated by
     * `ACTION_NULL`)
     */
    struct action *press_actions;
    /* the actions to execute when the button/key is released (terminated by
     * `ACTION_NULL`)
     */
    struct action *release_actions;
};

/* key bindings, the first index is the adjusted key code, the other is the
 * adjusted modifiers (modifiers without lock mask)
 */
static struct binding key_bindings[256 - 8][256 >> 1];

/* Dynamic array of button bindings.  There is no compile time constant to tell
 * us a maximum value.  The second index is the adjusted modifiers like above.
 */
static struct binding (*button_bindings)[256 >> 1];
static unsigned button_bindings_length;

/* Remove all ignored modifiers and the LOCK mask and shift the bits into the
 * LOCK mask.
 */
static xkb_mod_mask_t adjust_modifiers(xkb_mod_mask_t modifiers)
{
    /* we ignore XCB_MOD_MASK_LOCK by default but also all bits above 0xff */
    unsigned ignore_modifiers = (~0xff | XCB_MOD_MASK_LOCK);

    /* ignore NumLock and ScrollLock modifiers */
    ignore_modifiers |= xkb_keymap_mod_get_mask(display.keymap,
            XKB_VMOD_NAME_NUM);
    ignore_modifiers |= xkb_keymap_mod_get_mask(display.keymap,
            XKB_VMOD_NAME_SCROLL);
    modifiers &= ~ignore_modifiers;

    /* get rid of the LOCK mask by combining the bits below and above */
    modifiers = ((modifiers & (XCB_MOD_MASK_LOCK - 1)) |
            ((modifiers & ~(XCB_MOD_MASK_LOCK - 1)) >> 1));

    return modifiers;
}

#ifdef DEBUG

/* Create a gap in the bits @modifiers at the LOCK mask. */
#define DEBUG_REVERSE_ADJUST(modifiers) \
    ((modifiers & (XCB_MOD_MASK_LOCK - 1)) | \
            ((modifiers & ~(XCB_MOD_MASK_LOCK - 1)) << 1))

/* Dump all bindings created to `stdout`. */
void debug_dump_bindings(void)
{
    struct binding *binding;

    for (unsigned kc = 0; kc < SIZE(key_bindings); kc++) {
        for (unsigned m = 0; m < SIZE(key_bindings[0]); m++) {
            binding = &key_bindings[kc][m];
            if (binding->press_actions != NULL) {
                printf("KP %u %u\n", DEBUG_REVERSE_ADJUST(m), kc + 8);
            }
            if (binding->release_actions != NULL) {
                printf("KR %u %u\n", DEBUG_REVERSE_ADJUST(m), kc + 8);
            }
        }
    }

    for (unsigned b = 0; b < button_bindings_length; b++) {
        for (unsigned m = 0; m < SIZE(button_bindings[0]); m++) {
            binding = &button_bindings[b][m];
            if (binding->press_actions != NULL) {
                printf("BP %u %u\n", DEBUG_REVERSE_ADJUST(m), b);
            }
            if (binding->release_actions != NULL) {
                printf("BR %u %u\n", DEBUG_REVERSE_ADJUST(m), b);
            }
        }
    }
}

#endif

/* Clear all bindings. */
void clear_bindings(void)
{
    struct binding *binding;

    for (unsigned kc = 0; kc < SIZE(key_bindings); kc++) {
        for (unsigned m = 0; m < SIZE(key_bindings[0]); m++) {
            binding = &key_bindings[kc][m];

            free(binding->press_actions);
            binding->press_actions = NULL;

            free(binding->release_actions);
            binding->release_actions = NULL;
        }
    }

    for (unsigned b = 0; b < button_bindings_length; b++) {
        for (unsigned m = 0; m < SIZE(button_bindings[0]); m++) {
            binding = &button_bindings[b][m];

            free(binding->press_actions);
            binding->press_actions = NULL;

            free(binding->release_actions);
            binding->release_actions = NULL;
        }
    }
}

/* Append an action to a binding.
 *
 * @binding may be NULL, then nothing happens.
 */
static void append_binding_action(_Nullable struct binding *binding,
        bool is_release, struct action action)
{
    unsigned length = 0;
    struct action *actions;

    if (binding != NULL) {
        if (is_release) {
            actions = binding->release_actions;
        } else {
            actions = binding->press_actions;
        }

        /* get the length of the action list */
        if (actions != NULL) {
            while (actions[length].type != ACTION_NULL) {
                length++;
            }
        }

        /* `length` is now the number of actions excluding `ACTION_NULL` so add
         * 2 for the new `action` and `ACTION_NULL`
         */
        REALLOCATE(actions, length + 2);
        actions[length] = action;
        length++;
        actions[length].type = ACTION_NULL;

        if (is_release) {
            binding->release_actions = actions;
        } else {
            binding->press_actions = actions;
        }
    }
}

/* Get a pointer to the binding corresponding to given modifiers and key code.
 */
static struct binding *get_key_binding_pointer(xkb_mod_mask_t modifiers,
        xkb_keycode_t key_code)
{
    modifiers = adjust_modifiers(modifiers);

    /* the X server does not support values outside this range */
    if (key_code < 8 || key_code >= 256) {
        return NULL;
    }

    key_code -= 8;

    return &key_bindings[key_code][modifiers];
}

/* Associate a key (with modifiers) on the keyboard with an action. */
void append_key_binding(bool is_release, xkb_mod_mask_t modifiers,
        xkb_keycode_t key_code, struct action action)
{
    struct binding *binding;

    binding = get_key_binding_pointer(modifiers, key_code);
    append_binding_action(binding, is_release, action);
}

/* struct to pass into `xkb_keymap_key_for_each()` for `set_bind_iterator()` */
struct key_iterator_context {
    /* current keyboard state to use */
    struct xkb_state *state;
    /* the key symbol to resolve */
    xkb_keysym_t key_symbol;
    /* information to pass to `set_key_binding()` */
    bool is_release;
    xkb_mod_mask_t modifiers;
    struct action action;
};

/* Iterator for key codes of a keymap. */
static void set_bind_iterator(struct xkb_keymap *keymap, xkb_keycode_t key_code,
        void *data)
{
    struct key_iterator_context *context;
    xkb_layout_index_t layout;
    xkb_level_index_t num_levels;
    const xkb_keysym_t *key_symbols;
    int count;

    context = data;

    /* for the layout the key code is on, try to find a match for a key symbol
     * on any level (modifier shift level)
     */
    layout = xkb_state_key_get_layout(context->state, key_code);
    num_levels = xkb_keymap_num_levels_for_key(keymap, key_code, layout);
    for (xkb_level_index_t level = 0; level < num_levels; level++) {
        count = xkb_keymap_key_get_syms_by_level(keymap, key_code, layout,
                level, &key_symbols);
        for (int i = 0; i < count; i++) {
            if (key_symbols[i] == context->key_symbol) {
                append_key_binding(context->is_release, context->modifiers,
                        key_code, context->action);
            }
        }
    }
}

/* Set a key binding using a key symbol. */
void append_key_symbol_binding(bool is_release, xkb_mod_mask_t modifiers,
        xkb_keysym_t key_symbol, struct action action)
{
    struct key_iterator_context key_iterator_context;

    key_iterator_context.state = display.keyboard_state;
    key_iterator_context.key_symbol = key_symbol;
    key_iterator_context.is_release = is_release;
    key_iterator_context.modifiers = modifiers;
    key_iterator_context.action = action;
    xkb_keymap_key_for_each(display.keymap, set_bind_iterator,
            &key_iterator_context);
}

/* Get action associated to a key binding. */
const struct action *get_key_binding(bool is_release, xkb_mod_mask_t modifiers,
        xkb_keycode_t key_code)
{
    struct binding *binding;
    struct action *actions = NULL;

    binding = get_key_binding_pointer(modifiers, key_code);
    if (binding != NULL) {
        if (is_release) {
            actions = binding->release_actions;
        } else {
            actions = binding->press_actions;
        }
    }
    return actions;
}

/* Get a pointer to the binding corresponding to given modifiers and button
 * combination.
 *
 * If the button is outside the currently allocated range, it is reallocated.
 */
static struct binding *get_button_binding_pointer(xkb_mod_mask_t modifiers,
        xcb_button_t button)
{
    modifiers = adjust_modifiers(modifiers);

    if (button >= button_bindings_length) {
        uint32_t new_length;

        new_length = button + 1;
        REALLOCATE(button_bindings, new_length);
        ZERO(&button_bindings[button_bindings_length],
                new_length - button_bindings_length);
        button_bindings_length = new_length;
    }

    return &button_bindings[button][modifiers];
}

/* Associate a button on the mouse or other device with an action. */
void append_button_binding(bool is_release, bool is_transparent,
        xkb_mod_mask_t modifiers, xcb_button_t button, struct action action)
{
    struct binding *binding;

    binding = get_button_binding_pointer(modifiers, button);
    if (binding != NULL) {
        append_binding_action(binding, is_release, action);
        if (is_transparent) {
            binding->flags |= BINDING_TRANSPARENT;
        }
    }
}

/* Get a list of actions associated to a button. */
const struct action *get_button_binding(bool is_release,
        xkb_mod_mask_t modifiers, xcb_button_t button,
        _Out bool *is_transparent)
{
    struct binding *binding;
    struct action *actions = NULL;

    binding = get_button_binding_pointer(modifiers, button);
    if (binding != NULL) {
        if (is_release) {
            actions = binding->release_actions;
        } else {
            actions = binding->press_actions;
        }
        *is_transparent = !!(binding->flags & BINDING_TRANSPARENT);
    }
    return actions;
}
