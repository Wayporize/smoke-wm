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

    /* the currently focused window, focus changes while a window is grabbed */
    xcb_window_t focus;

    /* last server timestamp usable for `WM_TAKE_FOCUS` client messages and
     * `SetInputFocus` requests
     */
    xcb_timestamp_t last_timestamp;

    /* the `WM_Sn` atom for the current screen */
    xcb_atom_t wm_sn_atom;
    /* the `MANAGER` atom */
    xcb_atom_t manager_atom;
    /* the window used for WM_Sn selection management */
    xcb_window_t wm_sn_window;

    /* base event for randr events */
    uint8_t randr_base_event, randr_base_error;

    /* xkb event and error identifiers */
    uint8_t xkb_base_event, xkb_base_error;
    /* id of the core keyboard device */
    int32_t keyboard_device_id;

    /* the `WM_PROTOCOLS` atom (list of atoms) */
    xcb_atom_t wm_protocols;
    /* the `WM_TAKE_FOCUS` atom (item of above list) */
    xcb_atom_t wm_take_focus;

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

enum wm_ownership_status {
    /* no problem occured acquiring the `WM_Sn` selection and setting up the
     * root event mask
     */
    WM_OWNERSHIP_SUCCESS,
    /* another manager interferred in acquiring the selection or setting up the
     * event mask
     */
    WM_OWNERSHIP_INTERFERRED,
    /* the present manager does not use `WM_sn`, there is nothing to do besides
     * waiting that this manager stops managing on its own
     */
    WM_OWNERSHIP_NONCOMPLIANT,
    /* the existing window manager took too long to destroy the manager window
     */
    WM_OWNERSHIP_TIMEOUT,
};

/* Try to become the window manager on the current X11 connection. */
enum wm_ownership_status take_wm_ownership(void);

/* Handle incoming events on the X11 connection.
 *
 * This function blocks until the user exits normally or an error occurs.
 */
void handle_server_events(void);

#endif
