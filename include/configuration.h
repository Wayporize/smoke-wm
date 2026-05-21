#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <stdbool.h>

#include <xcb/xproto.h>
#include <xcb/render.h>
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

/* the globally accessible configuration object */
extern struct wm {
    /* [wm.tiling] */
    struct wm_tiling {
        /* the layout to use by default */
        enum tiling_layout layout;

        /* [wm.tiling.gaps] */
        struct wm_tiling_gaps {
            /* gaps between tiled windows */
            int inner[4];
            /* gaps between tiled windows and the monitor edges */
            int outer[4];
        } gaps;
    } tiling;

    /* [wm.border] */
    struct wm_border {
        /* size in pixels of the border */
        int size;
        /* which decoration type to use by default */
        enum border_decoration decoration;

        /* [wm.border.radius] */
        struct wm_border_radius {
            /* the radius within the inside of the window */
            int inner;
            /* the radius of the outside of the window */
            int outer;
        } radius;

        /* [wm.border.color] */
        struct wm_border_color {
            /* for each color, alpha == 0 indicates that this value is not set
             */
            /* the color of the border when the window is focused */
            xcb_render_color_t focused;
            /* the secondary focused color of the border */
            xcb_render_color_t highlight;
            /* the color for in active windows (not focused, not highlighted) */
            xcb_render_color_t inactive;
            /* focused colors for floating and tiling windows */
            xcb_render_color_t floating, tiling;
        } color;
    } border;

    /* [[wm.monitor]] monitor layout specifications */
    LIST(struct wm_monitor {
        /* name of the monitor per Xrandr */
        char *name;
        /* layout to use for this monitor */
        enum tiling_layout layout;
    }, monitor);

    /* [[wm.workspace]] definition of specific workspaces */
    LIST(struct wm_workspace {
        /* string name of this workspace */
        char *name;
        /* unique number identifier */
        unsigned number;
        /* the monitor this workspace is supposed to be on */
        char *monitor;
        /* layout to use for this workspace */
        enum tiling_layout layout;
    }, workspace);

    /* [[wm.window]] */
    LIST(struct wm_window {
        /* the name pattern to match against */
        char *name;
        /* the class pattern to match against */
        char *class;
        /* the instance pattern to match against */
        char *instance;

        /* the workspace to appear on */
        char *workspace;
        /* the monitor to appear on */
        char *monitor;
        /* whether the window starts off hidden */
        bool hidden;
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
        /* mouse button; 0 if no button defined, otherwise 1+index */
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

#endif
