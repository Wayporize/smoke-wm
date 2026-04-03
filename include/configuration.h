#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <stdbool.h>

#include <xcb/xproto.h>
#include <xcb/render.h>
#include <xkbcommon/xkbcommon.h>

#include <utility/list.h>

/* TODO: put me somewhere else */
enum tiling_layout {
    TILE_UNSPECIFIED,
    TILE_MANUAL,
    TILE_HORIZONTAL,
    TILE_VERTICAL,
    TILE_GRID,
    TILE_SPIRAL
};

enum window_mode {
    WINDOW_UNSPECIFIED,
    WINDOW_TILING,
    WINDOW_FLOATING,
    WINDOW_FULLSCREEN
};

enum border_decoration {
    BORDER_NONE,
    BORDER_SIMPLE,
    BORDER_FULL,
};

enum action_type {
    ACTION_NONE
};

union action_value {
    int value;
};

/* the globally accessible configuration object */
extern struct wm {
    /* [wm.gaps] */
    struct wm_gaps {
        int inner[4];
        int outer[4];
    } gaps;

    /* [wm.border] */
    struct wm_border {
        int size;
        enum border_decoration decoration;

        struct wm_border_radius {
            int inner;
            int outer;
        } radius;

        struct wm_border_color {
            xcb_render_color_t focused;
            xcb_render_color_t highlight;
            xcb_render_color_t inactive;
            xcb_render_color_t floating, tiling;
        } color;
    } border;

    /* [[wm.monitor]] monitor layout specifications */
    LIST(struct wm_monitor {
        char *name;
        enum tiling_layout layout;
    }, monitor);

    /* [[wm.workspace]] definition of specific workspaces */
    LIST(struct wm_workspace {
        char *name;
        unsigned number;
        char *monitor;
        enum tiling_layout layout;
    }, workspace);

    /* TODO: make this a hashmap */
    /* [[wm.window]] */
    LIST(struct wm_window {
        char *name;
        char *class;
        char *instance;
        char *workspace;
        char *monitor;
        bool hidden;
        enum window_mode mode;
        struct wm_border border;
    }, window);

    /* TODO: make this a hashmap */
    /* [[wm.binding]] / [wm.bindings] */
    LIST(struct wm_binding {
        bool is_release;
        bool is_transparent;
        uint16_t modifiers;
        xkb_keysym_t key_symbol;
        xkb_keycode_t key_code;
        /* 0 if no button defined, otherwise 1+index */
        xcb_button_t button;
        enum action_type action;
        union action_value value;
    }, binding);

    /* [[wm.startup]] */
    LIST(struct wm_startup {
        enum action_type action;
        union action_value value;
    }, startup);
} Configuration;

/* Clear a configuration object. */
void clear_configuration(struct wm *wm);

/* Get the path of the configuration to use on startup. */
char *get_configuration_path(void);

#endif
