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
    ASSERT(display.keymap != NULL, "could not create xkb keymap\n");

    display.keyboard_state = xkb_x11_state_new_from_device(display.keymap,
            display.xcb, display.keyboard_device_id);
    ASSERT(display.keyboard_state != NULL,
            "could not create xkb keyboard state\n");
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
    ASSERT(extension_reply != NULL,
            "failed to query xcb extension data for xkb\n");
    ASSERT(extension_reply->present,
            "xkb is not available on the server\n");

    display.xkb_base_event = extension_reply->first_event;
    display.xkb_base_error = extension_reply->first_error;

    reply = xcb_xkb_use_extension_reply(display.xcb, cookie, &error);
    ASSERT(reply != NULL, "using xcb extension xkb failed: error code %d\n",
            error->error_code);
    ASSERT(reply->supported, "server does not support xkb version %d.%d\n",
                XCB_XKB_MAJOR_VERSION, XCB_XKB_MINOR_VERSION);
    free(reply);

    display.xkb = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    ASSERT(display.xkb != NULL, "could not create xkb context\n");

    /* get the device id of the core keyboard */
    device_reply = xcb_xkb_get_device_info_reply(display.xcb, device_cookie,
            NULL);
    ASSERT(device_reply != NULL, "could not get xkb device info\n");
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
    ASSERT(client_reply != NULL, "could not set xkb per client flags\n");
    ASSERT((client_reply->value & XCB_XKB_PER_CLIENT_FLAG_DETECTABLE_AUTO_REPEAT),
            "could not set per client flags (X server can not comply)\n");
    free(client_reply);

    /* do an initial refresh */
    refresh_keyboard_mapping();
}

/* Get a string representation of a connection error. */
static const char *get_connection_error_string(int error)
{
    switch (error) {
    case 0:
        return "success";
    case XCB_CONN_ERROR:
        return "socket, pipe or other error";
    case XCB_CONN_CLOSED_EXT_NOTSUPPORTED:
        return "extension not supported";
    case XCB_CONN_CLOSED_MEM_INSUFFICIENT:
        return "memory was insufficient";
    case XCB_CONN_CLOSED_REQ_LEN_EXCEED:
        return "exceeded request length that server accepts";
    case XCB_CONN_CLOSED_PARSE_ERR:
        return "error parsing display string";
    case XCB_CONN_CLOSED_INVALID_SCREEN:
        return "server does not have a screen matching the display";
    default:
        return "unknown connection error";
    }
}

/* Open the X11 connection and initialize extensions. */
void open_display(void)
{
    int screen_index;
    int connection_error;
    const xcb_setup_t *setup;
    xcb_screen_iterator_t iterator;
    xcb_xkb_use_extension_cookie_t xkb_cookie;
    xcb_xkb_get_device_info_cookie_t xkb_device_cookie;
    xcb_xkb_per_client_flags_cookie_t xkb_client_cookie;

    /* connect to the X server */
    display.xcb = xcb_connect(NULL, &screen_index);
    connection_error = xcb_connection_has_error(display.xcb);
    ASSERT(connection_error == 0, "%s\n",
            get_connection_error_string(connection_error));

    display.screen_index = screen_index;
    setup = xcb_get_setup(display.xcb);
    for (iterator = xcb_setup_roots_iterator(setup);
            iterator.rem > 0; screen_index--, xcb_screen_next(&iterator)) {
        if (screen_index == 0) {
            display.screen = iterator.data;
            display.root = display.screen->root;
            break;
        }
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

/* Get the owner of the selection specified by given atom. */
static xcb_window_t get_selection_owner(xcb_atom_t atom)
{
    xcb_get_selection_owner_cookie_t owner_cookie;
    xcb_get_selection_owner_reply_t *owner_reply;
    xcb_window_t owner;

    owner_cookie = xcb_get_selection_owner(display.xcb, atom);
    owner_reply = xcb_get_selection_owner_reply(display.xcb, owner_cookie,
            NULL);
    ASSERT(owner_reply != NULL, "could not get selection owner\n");

    owner = owner_reply->owner;
    free(owner_reply);

    return owner;
}

/* Handle an event by the xkb extension. */
static void handle_xkb_event(xcb_generic_event_t *generic_event)
{
    xcb_xkb_state_notify_event_t *event;
    enum xkb_state_component change;

    event = (xcb_xkb_state_notify_event_t*) generic_event;
    if (event->deviceID == display.keyboard_device_id) {
        switch (event->xkbType) {
        case XCB_XKB_NEW_KEYBOARD_NOTIFY:
        case XCB_XKB_MAP_NOTIFY:
            printf("xkb: mapping changed\n");
            refresh_keyboard_mapping();
            clear_bindings();
            set_configuration_bindings(&Configuration);
            break;

        case XCB_XKB_STATE_NOTIFY:
            /* update the xkb keyboard state */
            change = xkb_state_update_mask(display.keyboard_state,
                    event->baseMods, event->latchedMods, event->lockedMods,
                    event->baseGroup, event->latchedGroup, event->lockedGroup);
            if ((change & XKB_STATE_LAYOUT_EFFECTIVE)) {
                printf("xkb: layout changed\n");
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

/* Handle all extensions events.
 *
 * @event is the event to handle.
 *
 * @return 0 if the event was an extension event, otherwise non-zero.
 */
static int handle_extension_event(xcb_generic_event_t *event)
{
    int status = 1;
    int error;

    if (event != NULL && event->response_type == display.xkb_base_event) {
        handle_xkb_event(event);
        status = 0;
    }

    error = xcb_connection_has_error(display.xcb);
    ASSERT(error == 0, "xcb connection error: %s\n",
            get_connection_error_string(error));

    return status;
}

/* Try to become the window manager on the current X11 connection. */
void take_wm_control(void)
{
    xcb_window_t manager_window;
    xcb_generic_event_t *event;
    char *atom_name;
    xcb_intern_atom_cookie_t atom_cookie;
    xcb_intern_atom_reply_t *atom_reply;
    xcb_atom_t wm_sn_atom;
    xcb_window_t old_manager_window;
    xcb_timestamp_t timestamp = 0;
    xcb_window_t owner;

    const uint32_t root_mask = XCB_CW_EVENT_MASK;
    const uint32_t root_attributes[] = { XCB_EVENT_MASK_PROPERTY_CHANGE };

    const uint32_t managed_root_mask = XCB_CW_EVENT_MASK;
    const uint32_t managed_root_attributes[] = {
        XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
            XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT
    };

    const uint32_t manager_mask = XCB_CW_EVENT_MASK;
    const uint32_t manager_attributes[] = { XCB_EVENT_MASK_STRUCTURE_NOTIFY };

    /* listen for property notify events */
    xcb_change_window_attributes(display.xcb, display.root,
            root_mask, root_attributes);

    /* cause a property notify event but do not change anything */
    xcb_change_property(display.xcb, XCB_PROP_MODE_APPEND, display.root,
            XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, 0, NULL);

    /* create specific manager window as per ICCCM */
    manager_window = xcb_generate_id(display.xcb);
    xcb_create_window(display.xcb, XCB_COPY_FROM_PARENT, manager_window,
            display.root, -1, -1, 1, 1, 0, XCB_WINDOW_CLASS_INPUT_ONLY,
            XCB_COPY_FROM_PARENT, manager_mask, manager_attributes);

    /* prefetch the WM_Sn atom */
    atom_name = xasprintf("WM_S%u", display.screen_index);
    atom_cookie = xcb_intern_atom(display.xcb, false, strlen(atom_name),
            atom_name);

    /* get the timestamp from the property notify event */
    while (true) {
        xcb_flush(display.xcb);
        event = xcb_wait_for_event(display.xcb);
        if (handle_extension_event(event) != 0) {
            if (event->response_type == XCB_PROPERTY_NOTIFY) {
                /* got the property notify event we were looking for */
                timestamp = ((xcb_property_notify_event_t*) event)->time;
                free(event);
                break;
            }
        }
        free(event);
    }

    /* get the value of the WM_Sn atom */
    atom_reply = xcb_intern_atom_reply(display.xcb, atom_cookie, NULL);
    ASSERT(atom_reply != NULL, "could not intern %s atom\n", atom_name);
    free(atom_name);
    wm_sn_atom = atom_reply->atom;
    free(atom_reply);

    owner = get_selection_owner(wm_sn_atom);

    do {
        old_manager_window = owner;
        if (old_manager_window == XCB_NONE) {
            break;
        }

        xcb_change_window_attributes(display.xcb, old_manager_window,
                manager_mask, manager_attributes);
        owner = get_selection_owner(wm_sn_atom);
    } while (owner != old_manager_window);

    xcb_set_selection_owner(display.xcb, manager_window, wm_sn_atom, timestamp);
    owner = get_selection_owner(wm_sn_atom);
    ASSERT(owner == manager_window, "a 3rd manager interferred\n");

    if (old_manager_window != XCB_NONE) {
        /* wait until the old manager destroyed the manager window */
        printf("waiting until the old manager destroys the manager window...\n");
        while (true) {
            xcb_flush(display.xcb);
            event = xcb_wait_for_event(display.xcb);
            if (handle_extension_event(event) != 0) {
                if (event->response_type == XCB_DESTROY_NOTIFY &&
                        ((xcb_destroy_notify_event_t*) event)->window ==
                            old_manager_window) {
                    printf("...success\n");
                    free(event);
                    break;
                }
            }
            free(event);
        }
    }

    printf("taking over...\n");
    xcb_change_window_attributes(display.xcb, display.root,
            managed_root_mask, managed_root_attributes);
}

/* Handle an error that occured. */
static void handle_error(xcb_generic_error_t *error)
{
    if (error->error_code == XCB_ACCESS &&
            error->resource_id == display.root &&
            error->major_code == XCB_CHANGE_WINDOW_ATTRIBUTES) {
        ABORT("...could not access root window.  "
                "The running window manager is not complying to "
                "ICCCM section 2.8\n");
    }
    printf("error: %u\n", error->error_code);
}

/* Handle a map request issued then a client called MapWindow. */
void handle_map_request(xcb_map_request_event_t *event)
{
    printf("got map request: 0x%x\n", event->window);
}

/* Handle incoming events on the X11 connection. */
void handle_server_events(void)
{
    xcb_generic_event_t *event;
    int status;

    do {
        /* flush so all requests are sent out before the next iteration */
        xcb_flush(display.xcb);
        /* block until another xcb event arrives */
        event = xcb_wait_for_event(display.xcb);
        /* check for extension events */
        status = handle_extension_event(event);
        if (status != 0) {
            switch (event->response_type) {
            case XCB_NONE:
                handle_error((xcb_generic_error_t*) event);
                break;

            case XCB_MAP_REQUEST:
                handle_map_request((xcb_map_request_event_t*) event);
                break;

            default:
                printf("event: %u\n", event->response_type);
                /* TODO: Handle core X11 events */
            }
            status = 0;
        }
        free(event);
    } while (status == 0);
}
