#include <stdlib.h>
#include <stdio.h>
#include <xcb/xkb.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-x11.h>

#include "binding.h"
#include "configuration.h"
#include "x11.h"

/* the information retrieved from the X server and Xkb context */
struct display display;

/* When an event by Xkb arrives indicating that the keyboard mapping changes,
 * this takes according actions to refresh the keymap and keyboard state.
 */
static void refresh_keyboard_mapping(void)
{
    xkb_state_unref(display.keyboard_state);
    xkb_keymap_unref(display.keymap);

    display.keymap = xkb_x11_keymap_new_from_device(display.xkb, display.xcb,
            display.keyboard_device_id, 0);
    if (display.keymap == NULL) {
        printf("could not create xkb keymap\n");
        exit(1);
    }

    display.keyboard_state = xkb_x11_state_new_from_device(display.keymap,
            display.xcb, display.keyboard_device_id);
    if (display.keyboard_state == NULL) {
        printf("could not create xkb keyboard state\n");
        exit(1);
    }
}

/* Open the X11 connection and initialize Xkb. */
void open_display(void)
{
    int connection_error;
    xcb_xkb_use_extension_cookie_t xkb_cookie;
    xcb_xkb_get_device_info_cookie_t device_cookie;
    const xcb_query_extension_reply_t *extension_reply;
    xcb_xkb_use_extension_reply_t *xkb_reply;
    xcb_xkb_get_device_info_reply_t *device_reply;
    xcb_generic_error_t *error;

    /* connect to the X server */
    display.xcb = xcb_connect(NULL, NULL);
    connection_error = xcb_connection_has_error(display.xcb);
    switch (connection_error) {
    case XCB_CONN_ERROR:
        printf("socket, pipe or other error\n");
        exit(1);
    case XCB_CONN_CLOSED_EXT_NOTSUPPORTED:
        printf("extension not supported\n");
        exit(1);
    case XCB_CONN_CLOSED_MEM_INSUFFICIENT:
        printf("memory was insufficient\n");
        exit(1);
    case XCB_CONN_CLOSED_REQ_LEN_EXCEED:
        printf("exceeded request length that server accepts\n");
        exit(1);
    case XCB_CONN_CLOSED_PARSE_ERR:
        printf("error parsing display string\n");
        exit(1);
    case XCB_CONN_CLOSED_INVALID_SCREEN:
        printf("server does not have a screen matching the display\n");
        exit(1);
    }

    xcb_prefetch_extension_data(display.xcb, &xcb_xkb_id);

    xkb_cookie = xcb_xkb_use_extension(display.xcb,
            XCB_XKB_MAJOR_VERSION, XCB_XKB_MINOR_VERSION);

    device_cookie = xcb_xkb_get_device_info(display.xcb,
            XCB_XKB_ID_USE_CORE_KBD, 0, 0, 0, 0, 0, 0);

    extension_reply = xcb_get_extension_data(display.xcb, &xcb_xkb_id);
    if (extension_reply == NULL) {
        printf("failed to query xcb extension data for xkb\n");
        exit(1);
    } else if (!extension_reply->present) {
        printf("xkb is not available on the server\n");
        exit(1);
    }

    display.xkb_base_event = extension_reply->first_event;
    display.xkb_base_error = extension_reply->first_error;

    xkb_reply = xcb_xkb_use_extension_reply(display.xcb, xkb_cookie, &error);
    if (xkb_reply == NULL) {
        printf("using xcb extension xkb failed: error code %d\n",
                error->error_code);
        free(error);
        exit(1);
    } else if (!xkb_reply->supported) {
        printf("server does not support xkb version %d.%d\n",
                XCB_XKB_MAJOR_VERSION, XCB_XKB_MINOR_VERSION);
        exit(1);
    }

    free(xkb_reply);

    display.xkb = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (display.xkb == NULL) {
        printf("could not create xkb context\n");
        exit(1);
    }

    device_reply = xcb_xkb_get_device_info_reply(display.xcb, device_cookie,
            NULL);
    if (device_reply == NULL) {
        printf("could not get xkb device info\n");
        exit(1);
    }
    display.keyboard_device_id = device_reply->deviceID;
    free(device_reply);

    /* select for state and map notify events */
    xcb_xkb_select_events(display.xcb, display.keyboard_device_id,
                                       XCB_XKB_EVENT_TYPE_STATE_NOTIFY |
                                       XCB_XKB_EVENT_TYPE_NEW_KEYBOARD_NOTIFY |
                                       /* TODO: I do not know how to trigger
                                        * this event and what it does
                                        */
                                       XCB_XKB_EVENT_TYPE_MAP_NOTIFY,
                                       /* TODO: Is it apparent that I have no
                                        * clue how to use these?
                                        */
                                       0, ~0, ~0, ~0, NULL);
    /* do an initial refresh */
    refresh_keyboard_mapping();
}

/* Handle an event by the xkb extension. */
static void handle_xkb_event(xcb_generic_event_t *generic_event)
{
    switch (((xcb_xkb_new_keyboard_notify_event_t*) generic_event)->xkbType) {
    case XCB_XKB_NEW_KEYBOARD_NOTIFY: {
        xcb_xkb_new_keyboard_notify_event_t *event;

        event = (xcb_xkb_new_keyboard_notify_event_t*) generic_event;
        /* we also get new keyboard notifications for other keyboards but we
         * only care about the X core keyboard
         */
        if (event->oldDeviceID == display.keyboard_device_id) {
            /* TODO: does this ever change? */
            if ((event->changed & XCB_XKB_NKN_DETAIL_DEVICE_ID)) {
                display.keyboard_device_id = event->deviceID;
            }

            if ((event->changed & XCB_XKB_NKN_DETAIL_KEYCODES)) {
                refresh_keyboard_mapping();
                clear_bindings();
                set_configuration_bindings(&Configuration);
            }

            /* the geometry can also change (XCB_XKB_NKN_DETAIL_GEOMETRY) but
             * this is not important to us
             */
        }
        break;
    }

    case XCB_XKB_MAP_NOTIFY: {
        xcb_xkb_map_notify_event_t *event;

        event = (xcb_xkb_map_notify_event_t*) generic_event;
        (void) event /* TODO: */;
        /* TODO: when is a refresh needed?
         * My guess is XCB_XKB_MAP_PART_* constants should be used
         */
        refresh_keyboard_mapping();
        clear_bindings();
        set_configuration_bindings(&Configuration);
        break;
    }

    case XCB_XKB_STATE_NOTIFY: {
        xcb_xkb_state_notify_event_t *event;
        enum xkb_state_component change;

        event = (xcb_xkb_state_notify_event_t*) generic_event;
        /* update the xkb keyboard state */
        change = xkb_state_update_mask(display.keyboard_state,
                event->baseMods, event->latchedMods, event->lockedMods,
                event->baseGroup, event->latchedGroup, event->lockedGroup);
        if ((change & XKB_STATE_LAYOUT_EFFECTIVE)) {
            /* layout has changed, simply re-create the bindings with the new
             * layout in the keyboard state
             */
            clear_bindings();
            set_configuration_bindings(&Configuration);
        }
        break;
    }
    }
}

/* Handle incoming events on the X11 connection. */
void handle_server_events(void)
{
    xcb_generic_event_t *event;
    int error;

    /* do an initial flush so all requests are sent out before entering the
     * event loop
     */
    xcb_flush(display.xcb);

    while (event = xcb_wait_for_event(display.xcb), event != NULL) {
        if (event->response_type == display.xkb_base_event) {
            handle_xkb_event(event);
        } else {
            printf("event: %u\n", event->response_type);
            switch (event->response_type) {
                /* TODO: Handle more X11 events */
            }
        }
        free(event);
    }

    error = xcb_connection_has_error(display.xcb);
    if (error > 0) {
        printf("xcb connection error occured\n");
        exit(1);
    }
}
