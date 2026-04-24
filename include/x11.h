#ifndef X11_H
#define X11_H

#include <xcb/xcb.h>
#include <xkbcommon/xkbcommon.h>

struct display {
    /* connection to the X server */
    xcb_connection_t *xcb;
    /* currently active screen */
    xcb_screen_t *screen;
    unsigned screen_index;
    /* root window on the active screen */
    xcb_window_t root;

    /* the WM_Sn atom for the current screen */
    xcb_atom_t wm_sn_atom;
    /* the MANAGER atom */
    xcb_atom_t manager_atom;
    /* the window used for WM_Sn selection management */
    xcb_window_t wm_sn_window;

    /* xkb event and error identifiers */
    uint8_t xkb_base_event, xkb_base_error;
    /* id of the core keyboard device */
    int32_t keyboard_device_id;

    /* xkb context */
    struct xkb_context *xkb;
    /* xkb keymap */
    struct xkb_keymap *keymap;
    /* xkb keyboard state */
    struct xkb_state *keyboard_state;
};

/* the information retrieved from the X server and extension context */
extern struct display display;

/* Open the X11 connection and initialize extensions.
 *
 * This function exits if an error occured.
 */
void open_display(void);

/* Try to become the window manager on the current X11 connection.
 *
 * If this fails, the program exits.
 */
void take_wm_ownership(void);

/* Handle incoming events on the X11 connection.
 *
 * This function blocks until the user exits normally or an error occurs.
 */
void handle_server_events(void);

#endif
