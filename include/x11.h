#ifndef X11_H
#define X11_H

#include <xcb/xcb.h>
#include <xkbcommon/xkbcommon.h>

struct display {
    /* connection to the X server */
    xcb_connection_t *xcb;
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

/* Handle incoming events on the X11 connection.
 *
 * This function blocks until the user exits normally or an error occurs.
 */
void handle_server_events(void);

#endif
