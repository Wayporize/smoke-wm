#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <stdbool.h>

#include <xkbcommon/xkbcommon.h>

#include <utility/list.h>

#include "action.h"
#include "tiling.h"
#include "window.h"

/* TODO: put me somewhere else */

enum window_mode {
    WINDOW_UNSPECIFIED,
    WINDOW_TILING,
    WINDOW_FLOATING,
    WINDOW_FULLSCREEN
};

enum border_decoration {
    BORDER_UNSPECIFIED,
    BORDER_NONE,
    BORDER_SIMPLE,
    BORDER_FULL
};

struct wm_color {
    /* if this actually has a value */
    bool is_set;
    /* color transparency, TODO: relevance? */
    uint16_t alpha;
    /* red, green and blue components */
    uint16_t red, green, blue;
};

/* the globally accessible configuration object */
extern struct wm {
    /* [wm.tiling] */
    struct wm_tiling {
        /* the layout to use by default */
        enum tiling_layout layout;

        /* [wm.tiling.gaps] */
        struct wm_tiling_gaps {
            /* gaps between tiled windows */
            int32_t inner[4];
            /* gaps between tiled windows and the output edges */
            int32_t outer[4];
        } gaps;
    } tiling;

    /* [wm.border] */
    struct wm_border {
        /* size in pixels of the border */
        int32_t size;
        /* which decoration type to use by default */
        enum border_decoration decoration;

        /* [wm.border.radius] */
        struct wm_border_radius {
            /* the radius within the inside of the window */
            int32_t inner;
            /* the radius of the outside of the window */
            int32_t outer;
        } radius;

        /* [wm.border.color] */
        struct wm_border_color {
            /* the color of the border when the window is focused */
            struct wm_color focused;
            /* the secondary focused color of the border */
            struct wm_color highlight;
            /* the color for in active windows (not focused, not highlighted) */
            struct wm_color inactive;
            /* focused colors for floating and tiling windows */
            struct wm_color floating, tiling;
        } color;
    } border;

    /* [[wm.output]] output layout specifications */
    LIST(struct wm_output {
        /* name of the output specified by RandR */
        utf8_t *name;
        /* layout to use for this output */
        enum tiling_layout layout;
    }, output);

    /* [[wm.workspace]] definition of specific workspaces */
    LIST(struct wm_workspace {
        /* string name of this workspace */
        utf8_t *name;
        /* the output this workspace is supposed to be on */
        utf8_t *output;
        /* layout to use for this workspace */
        enum tiling_layout layout;
    }, workspace);

    /* [[wm.window]] */
    LIST(struct wm_window {
        /* the name pattern to match against */
        utf8_t *name;
        /* the class pattern to match against */
        utf8_t *class;
        /* the instance pattern to match against */
        utf8_t *instance;

        /* the workspace to appear on */
        utf8_t *workspace;
        /* the output to appear on */
        utf8_t *output;
        /* whether the window starts off hidden
         * (-1 for "unset", 0/1 for false/true)
         */
        int hidden;
        /* user chosen mode to overwrite the default mode */
        enum window_mode mode;
        /* specific border for this window */
        struct wm_border border;
    }, window);

    /* [[wm.binding]] / [wm.bindings] */
    LIST(struct wm_binding {
        /* if the key/button needs to be released for this binding to trigger */
        bool is_release;
        /* if button presses pass through to the underlying window */
        bool is_transparent;
        /* the modifiers needed in addition to the key/button */
        xkb_mod_mask_t modifiers;
        /* the key symbol */
        xkb_keysym_t key_symbol;
        /* the key code (usually specific key on the keyboard) */
        xkb_keycode_t key_code;
        /* mouse button; 0 if no button defined, otherwise the button index */
        xcb_button_t button;
        /* the action to trigger */
        enum action_type action;
        /* the value of the action, not every action has this */
        union action_value value;
    }, binding);

    /* [[wm.startup]] */
    LIST(struct wm_startup {
        /* the action to trigger at startup */
        enum action_type action;
        /* the value of the action, not every action has this */
        union action_value value;
    }, startup);
} Configuration, Configuration_default;

/* Dump all configuration options to stdout. */
void dump_configuration(struct wm *wm);

/* Clear a configuration object. */
void clear_configuration(struct wm *wm);

/* Set the bindings of a configuration as global bindings.
 *
 * Use `clear_bindings()` to remove them all again.
 */
void set_configuration_bindings(struct wm *wm);

/* Get the path of the configuration to use on startup.
 *
 * @return NULL if there is no configuration file.
 */
char *get_configuration_path(void);

/* Get the configuration entry associated to given window.
 *
 * @configuration will hold the window configuration.
 *
 * @return whether the window has any configuration.
 */
bool get_window_configuration(const struct window *window, struct wm_window *configuration);

#endif
