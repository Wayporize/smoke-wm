#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <unistd.h>
#include <utility/log.h>
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

    char *wm_sn_atom_name;
    const char *const manager_atom_name = "MANAGER";
    xcb_intern_atom_cookie_t wm_sn_atom_cookie, manager_atom_cookie;
    xcb_intern_atom_reply_t *wm_sn_atom_reply, *manager_atom_reply;

    xcb_xkb_use_extension_cookie_t xkb_cookie;
    xcb_xkb_get_device_info_cookie_t xkb_device_cookie;
    xcb_xkb_per_client_flags_cookie_t xkb_client_cookie;

    const uint32_t root_mask = XCB_CW_EVENT_MASK;
    const uint32_t root_attributes[] = { XCB_EVENT_MASK_PROPERTY_CHANGE };

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

    /* prefetch the WM_Sn atom and MANAGER atom */
    wm_sn_atom_name = xasprintf("WM_S%u", display.screen_index);
    wm_sn_atom_cookie = xcb_intern_atom(display.xcb, false,
            strlen(wm_sn_atom_name), wm_sn_atom_name);
    manager_atom_cookie = xcb_intern_atom(display.xcb, false,
            strlen(manager_atom_name), manager_atom_name);

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

    /* get the value of the WM_Sn atom */
    wm_sn_atom_reply = xcb_intern_atom_reply(display.xcb, wm_sn_atom_cookie,
            NULL);
    ASSERT(wm_sn_atom_reply != NULL, "could not intern %s atom\n",
            wm_sn_atom_name);
    free(wm_sn_atom_name);
    display.wm_sn_atom = wm_sn_atom_reply->atom;
    free(wm_sn_atom_reply);
    /* get the value of the MANAGER atom */
    manager_atom_reply = xcb_intern_atom_reply(display.xcb, manager_atom_cookie,
            NULL);
    ASSERT(manager_atom_reply != NULL, "could not intern %s atom\n",
            manager_atom_name);
    display.manager_atom = manager_atom_reply->atom;
    free(manager_atom_reply);

    initialize_xkb(xkb_cookie, xkb_device_cookie, xkb_client_cookie);

    /* the root window starts off with property notifications and will always
     * listen for property nofitications to get server timestamps
     */
    xcb_change_window_attributes(display.xcb, display.root,
            root_mask, root_attributes);
}

/* Handle an error that occured. */
static void handle_error(xcb_generic_error_t *error)
{
    notef("error: %u\n", error->error_code);
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
            notef("xkb: mapping changed\n");
            refresh_keyboard_mapping();
            clear_bindings();
            set_configuration_bindings(&Configuration);
            /* immediately send our requests */
            xcb_flush(display.xcb);
            break;

        case XCB_XKB_STATE_NOTIFY:
            /* update the xkb keyboard state */
            change = xkb_state_update_mask(display.keyboard_state,
                    event->baseMods, event->latchedMods, event->lockedMods,
                    event->baseGroup, event->latchedGroup, event->lockedGroup);
            if ((change & XKB_STATE_LAYOUT_EFFECTIVE)) {
                notef("xkb: layout changed\n");
                /* layout has changed, simply re-create the bindings with the new
                 * layout in the keyboard state
                 */
                clear_bindings();
                set_configuration_bindings(&Configuration);
                /* immediately send our requests */
                xcb_flush(display.xcb);
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

    if (event != NULL) {
        if (event->response_type == XCB_NONE) {
            handle_error((xcb_generic_error_t*) event);
            status = 0;
        } else if (event->response_type == display.xkb_base_event) {
            handle_xkb_event(event);
            status = 0;
        }
    }

    error = xcb_connection_has_error(display.xcb);
    ASSERT(error == 0, "xcb connection error: %s\n",
            get_connection_error_string(error));

    return status;
}

/* Get a timestamp from the server using an empty property append. */
static xcb_timestamp_t get_server_timestamp(void)
{
    xcb_generic_event_t *event;
    xcb_property_notify_event_t *notify;
    int status;
    xcb_timestamp_t timestamp = 0;

    /* cause a property notify event but do not change anything */
    xcb_change_property(display.xcb, XCB_PROP_MODE_APPEND, display.root,
            XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, 0, NULL);
    /* flush out the request */
    xcb_flush(display.xcb);

    /* get the timestamp from the property notify event */
    while (event = xcb_wait_for_event(display.xcb), event != NULL) {
        notify = (xcb_property_notify_event_t*) event;
        status = handle_extension_event(event);
        /* TODO: for now this can only be from the root, make sure to
         * properly handle the case where property notify events can
         * come in from other sources
         */
        if (status != 0 &&
                notify->response_type == XCB_PROPERTY_NOTIFY &&
                notify->atom == XCB_ATOM_WM_NAME &&
                notify->state == XCB_PROPERTY_NEW_VALUE) {
            /* got the property notify event we were looking for */
            timestamp = notify->time;
            free(event);
            break;
        }
        free(event);
    }

    return timestamp;
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

/* Change the event mask such that destroy notifications are sent.
 *
 * It can happen that the manager window changes while changing the mask.
 * The new window is returned and should be used instead as the new owner.
 */
static xcb_window_t change_selection_owner_event_mask_to_destruction(
        xcb_window_t owner, xcb_atom_t atom)
{
    const uint32_t manager_mask = XCB_CW_EVENT_MASK;
    const uint32_t manager_attributes[] = { XCB_EVENT_MASK_STRUCTURE_NOTIFY };

    xcb_window_t old_owner;

    do {
        if (owner == XCB_NONE) {
            break;
        }

        xcb_change_window_attributes(display.xcb, owner,
                manager_mask, manager_attributes);

        /* check if the owner changed */
        old_owner = owner;
        owner = get_selection_owner(atom);
    } while (owner != old_owner);

    return owner;
}

/* Wait until the given window was destroyed for up to 5 seconds. */
static int wait_for_destroy_notification(xcb_window_t window)
{
    int file_descriptor;
    struct timeval timeout;
    bool is_destroy_notification_received = false;
    fd_set read_set;
    int return_value;
    xcb_generic_event_t *event;
    xcb_destroy_notify_event_t *notify;
    int status;

    file_descriptor = xcb_get_file_descriptor(display.xcb);

    /* wait for up to 5 seconds */
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    while (!is_destroy_notification_received) {
        FD_ZERO(&read_set);
        FD_SET(file_descriptor, &read_set);
        return_value = select(file_descriptor + 1, &read_set,
                NULL, NULL, &timeout);
        if (return_value <= 0) {
            break;
        }

        /* read all received events (might be none) */
        while (event = xcb_poll_for_event(display.xcb), event != NULL) {
            notify = (xcb_destroy_notify_event_t*) event;
            status = handle_extension_event(event);
            if (status != 0 &&
                    event->response_type == XCB_DESTROY_NOTIFY &&
                    notify->window == window) {
                notef("...success\n");
                free(event);
                is_destroy_notification_received = true;
                break;
            }
            free(event);
        }
    }

    if (!is_destroy_notification_received) {
        notef("...failed: timeout\n");
        return_value = 1;
    } else {
        return_value = 0;
    }

    return return_value;
}

/* Try to become the window manager on the current X11 connection.
 *
 * This takes ownership of the `WM_Sn` selection and selects the
 * `SubstructureRedirect` event mask on the root window to redirect certain
 * events for window management.
 */
enum wm_ownership_status take_wm_ownership(void)
{
    xcb_timestamp_t timestamp;
    xcb_window_t owner, previous_owner;
    enum wm_ownership_status status = WM_OWNERSHIP_SUCCESS;

    const uint32_t managed_root_mask = XCB_CW_EVENT_MASK;
    const uint32_t managed_root_attributes[] = {
        XCB_EVENT_MASK_PROPERTY_CHANGE |
        XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
            XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT
    };
    xcb_client_message_event_t message;
    xcb_void_cookie_t cookie;
    xcb_generic_error_t *error;

    /* create specific manager window as per ICCCM */
    display.wm_sn_window = xcb_generate_id(display.xcb);
    xcb_create_window(display.xcb, XCB_COPY_FROM_PARENT,
            display.wm_sn_window, display.root, -1, -1, 1, 1, 0,
            XCB_WINDOW_CLASS_INPUT_ONLY, XCB_COPY_FROM_PARENT, 0, NULL);
    notef("created selection window %#x\n", display.wm_sn_window);

    previous_owner = get_selection_owner(display.wm_sn_atom);
    previous_owner = change_selection_owner_event_mask_to_destruction(
            previous_owner, display.wm_sn_atom);

    timestamp = get_server_timestamp();
    xcb_set_selection_owner(display.xcb, display.wm_sn_window,
            display.wm_sn_atom, timestamp);

    /* make sure our selection owner request goes out */
    xcb_flush(display.xcb);

    notef("sent request to become the selection owner\n");

    /* wait until the previous owner destroys its owner window which means
     * (as specified in ICCCM 2.8) that we can now select for substructure
     * redirect events on the root window
     */
    if (previous_owner == XCB_NONE || (
            notef("waiting until the old manager destroys window %#x...\n",
                previous_owner),
            wait_for_destroy_notification(previous_owner) == 0)) {
        /* at this point, the owner might have already changed again */
        owner = get_selection_owner(display.wm_sn_atom);
        if (owner != display.wm_sn_window) {
            notef("a third manager interferred, can not take over\n");
            /* destroy the manager window to indicate to the interferring
             * manager that they are good to go
             */
            xcb_destroy_window(display.xcb, display.wm_sn_window);
            display.wm_sn_window = XCB_NONE;

            status = WM_OWNERSHIP_INTERFERRED;
        } else {
            /* send out a client message to announce that we are the new owner
             */
            message.response_type = XCB_CLIENT_MESSAGE;
            message.window = display.root;
            message.format = 32;
            message.type = display.manager_atom;
            message.data.data32[0] = timestamp;
            message.data.data32[1] = display.wm_sn_atom;
            message.data.data32[2] = display.wm_sn_window;
            xcb_send_event(display.xcb, false, display.root,
                    XCB_EVENT_MASK_STRUCTURE_NOTIFY, (char*) &message);

            /* redirect events to our window manager */
            cookie = xcb_change_window_attributes_checked(display.xcb,
                    display.root, managed_root_mask, managed_root_attributes);
            error = xcb_request_check(display.xcb, cookie);
            /* check for an error which can occur yet again because another
             * manager interferred
             */
            if (error != NULL) {
                if (error->error_code != XCB_ACCESS) {
                    ABORT("Could not change window attributes on the root window\n");
                }
                free(error);

                owner = get_selection_owner(display.wm_sn_atom);
                if (owner != XCB_NONE) {
                    /* a (maybe) ICCCM compliant manager stole away the mask
                     * while we were both selecting the owner at the same time
                     * so this was not spotted earlier
                     */
                    notef("a third manager interferred, can not select event mask\n");
                    status = WM_OWNERSHIP_INTERFERRED;
                } else {
                    /* someone has the mask but there is no owner */
                    notef("a non-ICCCM-compliant manager is present, can not overrule\n");
                    status = WM_OWNERSHIP_NONCOMPLIANT;
                }

                /* destroy the window again */
                xcb_destroy_window(display.xcb, display.wm_sn_window);
                display.wm_sn_window = XCB_NONE;
            } else {
                /* associated to a few manager tests */
                notef("taking over\n");
            }
        }
    }

    return status;
}

/* Handle a map request issued when a client called MapWindow. */
static void handle_map_request(xcb_map_request_event_t *event)
{
    notef("got map request: 0x%x\n", event->window);
    xcb_map_window(display.xcb, event->window);
}

/* Wait for the selection to become free again.
 *
 * We do this by waiting until all owners are destroyed.
 * 
 * @owner is the initial owner.
 */
static void go_dormant_and_wait_for_selection(xcb_window_t owner)
{
    xcb_generic_event_t *event;
    int status;
    xcb_destroy_notify_event_t *notify;

    /* associated to a few manager tests */
    notef("going dormant\n");

    /* listen for destroy notifications on the current owner */
    owner = change_selection_owner_event_mask_to_destruction(owner,
            display.wm_sn_atom);

    /* wait until there is no more owner */
    while (owner != XCB_NONE) {
        /* flush so all requests are sent out before the next iteration */
        xcb_flush(display.xcb);
        /* block until another xcb event arrives */
        event = xcb_wait_for_event(display.xcb);
        /* check for extension events */
        status = handle_extension_event(event);

        notify = (xcb_destroy_notify_event_t*) event;
        /* if the managing window is destroyed, there must be a new owner of
         * the selection
         */
        if (status != 0 &&
                notify->response_type == XCB_DESTROY_NOTIFY &&
                notify->window == owner) {
            /* get the new owner and listen for destroy notifications again */
            owner = get_selection_owner(display.wm_sn_atom);
            owner = change_selection_owner_event_mask_to_destruction(owner,
                    display.wm_sn_atom);
        }
        free(event);
    }
}

/* Handle losing a selection. */
static void handle_selection_clear(xcb_selection_clear_event_t *event)
{
    /* property change events are needed to get the server timestamp */
    const uint32_t root_mask = XCB_CW_EVENT_MASK;
    const uint32_t root_attributes[] = { XCB_EVENT_MASK_PROPERTY_CHANGE };

    xcb_window_t new_owner;
    enum wm_ownership_status status;

    if (event->owner == display.wm_sn_window &&
            event->selection == display.wm_sn_atom) {
        xcb_change_window_attributes(display.xcb, display.root,
                root_mask, root_attributes);

        /* destroy the manager window as required by ICCCM */
        xcb_destroy_window(display.xcb, display.wm_sn_window);
        display.wm_sn_window = XCB_NONE;

        do {
            /* get the new owner and go dormant */
            new_owner = get_selection_owner(display.wm_sn_atom);
            go_dormant_and_wait_for_selection(new_owner);
            notef("waking up\n");

            status = take_wm_ownership();
            if (status == WM_OWNERSHIP_NONCOMPLIANT) {
                /* add a small delay to not spam the user */
                sleep(1);
            }
        } while (status != WM_OWNERSHIP_SUCCESS);
    }
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
            case XCB_MAP_REQUEST:
                handle_map_request((xcb_map_request_event_t*) event);
                break;

            case XCB_SELECTION_CLEAR:
                handle_selection_clear((xcb_selection_clear_event_t*) event);
                break;

            default:
                notef("event: %u\n", event->response_type);
                /* TODO: Handle more core X11 events */
            }
            status = 0;
        }
        free(event);
    } while (status == 0);
}
