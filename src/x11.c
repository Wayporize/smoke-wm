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

/* Initialize the Xkb extension and xkbcommon library. */
static void initialize_xkb(xcb_xkb_use_extension_cookie_t cookie,
        xcb_xkb_get_device_info_cookie_t device_cookie,
        xcb_xkb_per_client_flags_cookie_t client_cookie)
{
    const xcb_query_extension_reply_t *extension_reply;
    xcb_generic_error_t *error;
    xcb_xkb_use_extension_reply_t *reply;
    xcb_xkb_get_device_info_reply_t *device_reply;
    xcb_xkb_select_events_details_t details;
    const uint16_t required_events = (XCB_XKB_EVENT_TYPE_STATE_NOTIFY |
            XCB_XKB_EVENT_TYPE_NEW_KEYBOARD_NOTIFY |
            XCB_XKB_EVENT_TYPE_MAP_NOTIFY);
    const uint16_t required_nkn_details = XCB_XKB_NKN_DETAIL_KEYCODES;
    const uint16_t required_map_parts = (XCB_XKB_MAP_PART_KEY_TYPES |
             XCB_XKB_MAP_PART_KEY_SYMS |
             XCB_XKB_MAP_PART_MODIFIER_MAP |
             XCB_XKB_MAP_PART_EXPLICIT_COMPONENTS |
             XCB_XKB_MAP_PART_KEY_ACTIONS |
             XCB_XKB_MAP_PART_VIRTUAL_MODS |
             XCB_XKB_MAP_PART_VIRTUAL_MOD_MAP);
    const uint16_t required_state_details = (XCB_XKB_STATE_PART_MODIFIER_BASE |
             XCB_XKB_STATE_PART_MODIFIER_LATCH |
             XCB_XKB_STATE_PART_MODIFIER_LOCK |
             XCB_XKB_STATE_PART_GROUP_BASE |
             XCB_XKB_STATE_PART_GROUP_LATCH |
             XCB_XKB_STATE_PART_GROUP_LOCK);
    xcb_xkb_per_client_flags_reply_t *client_reply;

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

    reply = xcb_xkb_use_extension_reply(display.xcb, cookie, &error);
    if (reply == NULL) {
        printf("using xcb extension xkb failed: error code %d\n",
                error->error_code);
        free(error);
        exit(1);
    } else if (!reply->supported) {
        printf("server does not support xkb version %d.%d\n",
                XCB_XKB_MAJOR_VERSION, XCB_XKB_MINOR_VERSION);
        exit(1);
    }

    free(reply);

    display.xkb = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (display.xkb == NULL) {
        printf("could not create xkb context\n");
        exit(1);
    }

    /* get the device id of the core keyboard */
    device_reply = xcb_xkb_get_device_info_reply(display.xcb, device_cookie,
            NULL);
    if (device_reply == NULL) {
        printf("could not get xkb device info\n");
        exit(1);
    }
    display.keyboard_device_id = device_reply->deviceID;
    free(device_reply);

    /* This duplication of values is because the first says which bits to affect
     * and the second one which value these bits should have.
     *
     * For example, the keyboard details we want to set are 0b001.
     * So we affect 0b001 and then set the values using 0b001 again.
     */
    ZERO(&details, 1);
    details.affectNewKeyboard = required_nkn_details;
    details.newKeyboardDetails = required_nkn_details;
    details.affectState = required_state_details;
    details.stateDetails = required_state_details;
    xcb_xkb_select_events_aux(display.xcb, display.keyboard_device_id,
            required_events,
            /* these are for enabling/disabling whole events, we use
             * details so we do not need them
             */
            0, 0,
            /* these are split off from `details`, not sure why they designed it
             * this way
             */
            required_map_parts, required_map_parts,
            &details);

    client_reply = xcb_xkb_per_client_flags_reply(display.xcb, client_cookie,
            &error);
    if (client_reply == NULL) {
        printf("could not set xkb per client flags\n");
        exit(1);
    } else {
        if (!(client_reply->value & XCB_XKB_PER_CLIENT_FLAG_DETECTABLE_AUTO_REPEAT)) {
            printf("could not set per client flags (X server can not comply)\n");
            exit(1);
        }
        free(client_reply);
    }

    /* do an initial refresh */
    refresh_keyboard_mapping();
}

/* Open the X11 connection and initialize Xkb. */
void open_display(void)
{
    int connection_error;
    xcb_xkb_use_extension_cookie_t xkb_cookie;
    xcb_xkb_get_device_info_cookie_t xkb_device_cookie;
    xcb_xkb_per_client_flags_cookie_t xkb_client_cookie;

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

    xkb_device_cookie = xcb_xkb_get_device_info(display.xcb,
            XCB_XKB_ID_USE_CORE_KBD, 0, 0, 0, 0, 0, 0);

    /* make it so when a key is held down and auto repeating, we can detect this
     * case instead of the core behaviour that just sends KeyPress/KeyRelease
     * pairs
     */
    xkb_client_cookie = xcb_xkb_per_client_flags(display.xcb,
            XCB_XKB_ID_USE_CORE_KBD,
            XCB_XKB_PER_CLIENT_FLAG_DETECTABLE_AUTO_REPEAT,
            XCB_XKB_PER_CLIENT_FLAG_DETECTABLE_AUTO_REPEAT,
            0, 0, 0);

    initialize_xkb(xkb_cookie, xkb_device_cookie, xkb_client_cookie);
}

/* Handle an event by the xkb extension. */
static void handle_xkb_event(xcb_generic_event_t *generic_event)
{
    /* ignore devices not concerning the core keyboard */
    if (((xcb_xkb_new_keyboard_notify_event_t*) generic_event)->deviceID !=
            display.keyboard_device_id) {
        return;
    }

    switch (((xcb_xkb_new_keyboard_notify_event_t*) generic_event)->xkbType) {
    case XCB_XKB_NEW_KEYBOARD_NOTIFY:
    case XCB_XKB_MAP_NOTIFY:
        refresh_keyboard_mapping();
        clear_bindings();
        set_configuration_bindings(&Configuration);
        break;

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
