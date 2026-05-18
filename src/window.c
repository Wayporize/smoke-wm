#include <inttypes.h>
#include <utility/list.h>
#include <utility/log.h>
#include <utility/utility.h>

#include <xcb/xcb_icccm.h>

#include "display.h"
#include "window.h"

/* list of all windows */
STATIC_LIST(struct window_cache, windows);

/* the currently focused window */
static xcb_window_t focused_window;

/* empty window */
static struct window_cache null_window;

/* Get a window cache by given X11 window id. */
struct window_cache *get_window_by_id(xcb_window_t id)
{
    for (size_t i = 0; i < windows_length; i++) {
        if (windows[i].id == id) {
            return &windows[i];
        }
    }

    notef("error: window %#x is not cached\n", id);
    return &null_window;
}

/* Create and register a new window from an X11 event. */
void create_window(xcb_create_notify_event_t *event)
{
    const uint32_t values[] = {
        XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_FOCUS_CHANGE
    };
    xcb_change_window_attributes(display.xcb, event->window,
            XCB_CW_EVENT_MASK, values);

    const struct window_cache window = {
        .id = event->window,
        .x = event->x, .y = event->y,
        .width = event->width, .height = event->height
    };
    LIST_APPEND_VALUE(windows, window);

    notef("window %#x creation registered\n", event->window);
}

/* Change a property of a window. */
void change_property(xcb_property_notify_event_t *event)
{
    struct window_cache *window;

    if (event->window == display.root) {
        notef("root property changed\n");
        return;
    }

    window = get_window_by_id(event->window);

#define REINSTANTIATE(atom_name, variable_name, function_name) \
    if (event->atom == atom_name) { \
        xcb_discard_reply(display.xcb, window->variable_name.cookie.sequence); \
        free(window->variable_name.reply); \
        window->variable_name.reply = NULL; \
        window->variable_name.cookie = function_name(display.xcb, event->window); \
    }

    REINSTANTIATE(XCB_ATOM_WM_NORMAL_HINTS, wm_normal_hints, xcb_icccm_get_wm_normal_hints)
    else REINSTANTIATE(XCB_ATOM_WM_HINTS, wm_hints, xcb_icccm_get_wm_hints)
    else if (event->atom == display.wm_protocols) {
        xcb_discard_reply(display.xcb, window->wm_protocols.cookie.sequence);
        free(window->wm_protocols.reply);
        window->wm_protocols.reply = NULL;
        window->wm_protocols.cookie = xcb_icccm_get_wm_protocols(display.xcb,
                event->window, display.wm_protocols);
    }

#undef REINSTANTIATE
}

/* Update a specific window property. */
int update_property(struct window_property *property)
{
    if (property->cookie.sequence != 0) {
        property->reply = xcb_get_property_reply(display.xcb,
                property->cookie, NULL);
        property->cookie.sequence = 0;
    }

    if (property->reply == NULL) {
        return 1;
    } else {
        return 0;
    }
}

/* Set an initial size using given size hints. */
static void set_initial_size(struct window_cache *window, xcb_size_hints_t *hints)
{
    if ((hints->flags & XCB_ICCCM_SIZE_HINT_US_POSITION)) {
        window->x = hints->x;
        window->y = hints->y;
    } else if ((hints->flags & XCB_ICCCM_SIZE_HINT_P_POSITION)) {
        window->x = hints->x;
        window->y = hints->y;
    } else {
        window->x = 0;
        window->y = 0;
    }

    if ((hints->flags & XCB_ICCCM_SIZE_HINT_US_SIZE)) {
        window->width = hints->width;
        window->height = hints->height;
    } else if ((hints->flags & XCB_ICCCM_SIZE_HINT_P_SIZE)) {
        window->width = hints->width;
        window->height = hints->height;
    } else if ((hints->flags & XCB_ICCCM_SIZE_HINT_BASE_SIZE)) {
        window->width = hints->base_width;
        window->height = hints->base_height;
    } else {
        window->width = hints->min_width;
        window->height = hints->min_height;
    }
}

/* Handle when a client wants to map (show) a window. */
void handle_map_request(xcb_map_request_event_t *event)
{
    struct window_cache *window;
    xcb_icccm_wm_hints_t hints;
    xcb_size_hints_t size_hints;
    xcb_icccm_wm_state_t old_state;
    xcb_icccm_get_wm_protocols_reply_t protocols;
    bool has_wm_take_focus = false;

    notef("got map request for %#x\n", event->window);

    window = get_window_by_id(event->window);

    if (update_property(&window->wm_hints) != 0 ||
            !xcb_icccm_get_wm_hints_from_reply(&hints,
                window->wm_hints.reply)) {
        ZERO(&hints, 1);
    }

    if (update_property(&window->wm_normal_hints) != 0 ||
            !xcb_icccm_get_wm_size_hints_from_reply(&size_hints,
                window->wm_normal_hints.reply)) {
        ZERO(&size_hints, 1);
    }

    if (update_property(&window->wm_protocols) != 0 ||
            !xcb_icccm_get_wm_protocols_from_reply(window->wm_protocols.reply,
                &protocols)) {
        ZERO(&protocols, 1);
    }

    old_state = window->state;
    if (window->state == XCB_ICCCM_WM_STATE_WITHDRAWN) {
        if ((hints.flags & XCB_ICCCM_WM_HINT_STATE)) {
            window->state = hints.initial_state;
        } else {
            window->state = XCB_ICCCM_WM_STATE_NORMAL;
        }
    } else if (window->state == XCB_ICCCM_WM_STATE_ICONIC) {
        window->state = XCB_ICCCM_WM_STATE_NORMAL;
    }

    notef("%#x: (%u, %u), (%u, %u), (%u, %u), (%u, %u), (%u, %u), (%u, %u), (%u, %u), (%u, %u), %u\n",
            size_hints.flags, size_hints.x, size_hints.y,
            size_hints.width, size_hints.height,
            size_hints.min_width, size_hints.min_height,
            size_hints.max_width, size_hints.max_height,
            size_hints.width_inc, size_hints.height_inc,
            size_hints.min_aspect_num, size_hints.min_aspect_den,
            size_hints.max_aspect_num, size_hints.max_aspect_den,
            size_hints.base_width, size_hints.base_height, size_hints.win_gravity);

    if (old_state == XCB_ICCCM_WM_STATE_WITHDRAWN) {
        set_initial_size(window, &size_hints);

        notef("configuring window %#x to "
                    "%" PRIi32 ", %" PRIi32 ", %" PRIi32 ", %" PRIi32 "\n",
                event->window, window->x, window->y, window->width, window->height);
        const uint16_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
        const uint32_t values[] = {
            window->x, window->y, window->width, window->height
        };
        xcb_configure_window(display.xcb, event->window, mask, values);
    }

    if (window->state == XCB_ICCCM_WM_STATE_NORMAL) {
        xcb_map_window(display.xcb, event->window);
    }
    xcb_flush(display.xcb);

    if (window->state == XCB_ICCCM_WM_STATE_NORMAL) {
        for (uint32_t i = 0; i < protocols.atoms_len; i++) {
            if (protocols.atoms[i] == display.wm_take_focus) {
                has_wm_take_focus = true;
                break;
            }
        }
        if (has_wm_take_focus) {
            xcb_client_message_event_t message;

            message.response_type = XCB_CLIENT_MESSAGE;
            message.window = event->window;
            message.format = 32;
            message.type = display.wm_protocols;
            message.data.data32[0] = display.wm_take_focus;
            message.data.data32[1] = display.last_timestamp;
            xcb_send_event(display.xcb, false, display.root,
                    XCB_EVENT_MASK_STRUCTURE_NOTIFY, (char*) &message);
        /* check if the window needs us to focus it directly */
        } else if (((hints.flags & XCB_ICCCM_WM_HINT_INPUT) && hints.input) ||
                    /* assume input = true if missing */
                    !(hints.flags & XCB_ICCCM_WM_HINT_INPUT)) {
            xcb_set_input_focus(display.xcb, XCB_INPUT_FOCUS_PARENT,
                    event->window, display.last_timestamp);
        }
        xcb_flush(display.xcb);
    }
}

/* Handle when a client wants to change the geometry or stacking of a window. */
void handle_configure_request(xcb_configure_request_event_t *event)
{
    struct window_cache *window;
    xcb_configure_notify_event_t *synthetic_event;

    window = get_window_by_id(event->window);

    synthetic_event = xmalloc(32);
    synthetic_event->response_type = XCB_CONFIGURE_NOTIFY;

    synthetic_event->event = event->window;
    synthetic_event->window = event->window;

    synthetic_event->above_sibling = XCB_NONE;

    synthetic_event->x = window->x;
    synthetic_event->y = window->y;
    synthetic_event->width = window->width;
    synthetic_event->height = window->height;
    synthetic_event->border_width = 0;

    synthetic_event->override_redirect = false;

    notef("sending synthetic configure event to %#" PRIx32 "\n", event->window);
    xcb_send_event(display.xcb, false, event->window,
            XCB_EVENT_MASK_STRUCTURE_NOTIFY, (char*) synthetic_event);
    free(synthetic_event);
    xcb_flush(display.xcb);
}

/* Tell the window module the new focused window. */
void report_focus_change(xcb_window_t window)
{
    notef("focus changed to %#" PRIx32 "\n", window);
}

/* Unregister a window. */
void destroy_window(xcb_destroy_notify_event_t *event)
{
    struct window_cache *window;

    window = get_window_by_id(event->window);
    if (window != &null_window) {
        windows_length--;
        const size_t index = window - windows;
        MOVE(window, window + 1, windows_length - index);
    }

    notef("window %#x destruction registered\n", event->window);
}
