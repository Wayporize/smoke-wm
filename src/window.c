#include <inttypes.h>
#include <utility/list.h>
#include <utility/log.h>
#include <utility/utility.h>

#include <xcb/xcb_icccm.h>

#include "display.h"
#include "window.h"
#include "workspace.h"

/* list of all windows */
STATIC_LIST(struct window_cache, windows);

/* empty window */
static struct window_cache null_window;

/* Get a window cache by given X11 window id. */
static struct window_cache *get_window_by_id(xcb_window_t id)
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
        .width = event->width, .height = event->height,
        .state = XCB_ICCCM_WM_STATE_NEW
    };
    LIST_APPEND_VALUE(windows, window);

    notef("window %#x creation registered\n", event->window);
}

/* Get the cached position and size of given X window. */
void get_window_rectangle(xcb_window_t id, struct rectangle *rectangle)
{
    struct window_cache *window;

    window = get_window_by_id(id);
    rectangle->x = window->x;
    rectangle->y = window->y;
    rectangle->width = window->width;
    rectangle->height = window->height;
}

/* Notify of a property change in a window. */
void change_property(xcb_property_notify_event_t *event)
{
    struct window_cache *window;

    if (event->window == display.root) {
        notef("root property changed\n");
        return;
    }

    window = get_window_by_id(event->window);
    (void) window;
    /* TODO: react to name changes and what not */
}

/* Set an initial size using given size hints. */
static void set_initial_size(struct window_cache *window)
{
    if ((window->normal_hints.flags & XCB_ICCCM_SIZE_HINT_US_POSITION)) {
        window->x = window->normal_hints.x;
        window->y = window->normal_hints.y;
    /* TODO: else choose a reasonable position or use the auto tiling */
    } else if ((window->normal_hints.flags & XCB_ICCCM_SIZE_HINT_P_POSITION)) {
        window->x = window->normal_hints.x;
        window->y = window->normal_hints.y;
    } else {
        window->x = 0;
        window->y = 0;
    }

    if ((window->normal_hints.flags & XCB_ICCCM_SIZE_HINT_US_SIZE)) {
        window->width = window->normal_hints.width;
        window->height = window->normal_hints.height;
    /* TODO: else choose a reasonable size or use the auto tiling */
    } else if ((window->normal_hints.flags & XCB_ICCCM_SIZE_HINT_P_SIZE)) {
        window->width = window->normal_hints.width;
        window->height = window->normal_hints.height;
    } else if ((window->normal_hints.flags & XCB_ICCCM_SIZE_HINT_BASE_SIZE)) {
        window->width = window->normal_hints.base_width;
        window->height = window->normal_hints.base_height;
    } else if ((window->normal_hints.flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE)) {
        window->width = window->normal_hints.min_width;
        window->height = window->normal_hints.min_height;
    } else {
        window->width = 16;
        window->height = 9;
    }
}

/* Focus a specific window in the X world.
 *
 * @return 0 if the window is focusable, 1 otherwise.
 */
static int focus_window(struct window_cache *window)
{
    /* send a client message if the `WM_TAKE_FOCUS` protocol is supported */
    if (window->protocols.has_wm_take_focus) {
        xcb_client_message_event_t message;

        message.response_type = XCB_CLIENT_MESSAGE;
        message.window = window->id;
        message.format = 32;
        message.type = display.wm_protocols;
        message.data.data32[0] = display.wm_take_focus;
        message.data.data32[1] = display.last_timestamp;
        xcb_send_event(display.xcb, false, window->id,
                XCB_EVENT_MASK_NO_EVENT, (char*) &message);
        return 0;
    /* check if the window needs us to focus it directly */
    } else if (((window->hints.flags & XCB_ICCCM_WM_HINT_INPUT) && window->hints.input) ||
                /* assume input = true if missing */
                !(window->hints.flags & XCB_ICCCM_WM_HINT_INPUT)) {
        /* `FOCUS_NONE` for full manual management */
        xcb_set_input_focus(display.xcb, XCB_INPUT_FOCUS_NONE,
                window->id, display.last_timestamp);
        return 0;
    }
    return 1;
}

/* Handle when a client wants to map (show) a window. */
void handle_map_request(xcb_map_request_event_t *event)
{
    struct window_cache *window;
    bool is_visible, is_workspace_visible;

    notef("got map request for %#x\n", event->window);

    window = get_window_by_id(event->window);

    /* check if the window is mapped for the first time */
    if (window->state == XCB_ICCCM_WM_STATE_NEW) {
        xcb_get_property_cookie_t hints_cookie, normal_hints_cookie, protocols_cookie;
        xcb_icccm_get_wm_protocols_reply_t protocols;

        hints_cookie = xcb_icccm_get_wm_hints(display.xcb, window->id);
        normal_hints_cookie = xcb_icccm_get_wm_normal_hints(display.xcb, window->id);
        protocols_cookie = xcb_icccm_get_wm_protocols(display.xcb, window->id, display.wm_protocols);

        (void) xcb_icccm_get_wm_hints_reply(display.xcb, hints_cookie, &window->hints, NULL);
        (void) xcb_icccm_get_wm_normal_hints_reply(display.xcb, normal_hints_cookie, &window->normal_hints, NULL);
        if (xcb_icccm_get_wm_protocols_reply(display.xcb, protocols_cookie, &protocols, NULL)) {
            for (uint32_t i = 0; i < protocols.atoms_len; i++) {
                if (protocols.atoms[i] == display.wm_take_focus) {
                    window->protocols.has_wm_take_focus = true;
                    notef("window supports WM_TAKE_FOCUS\n");
                }
            }
            xcb_icccm_get_wm_protocols_reply_wipe(&protocols);
        }

        notef("normal hints: %#x: (%u, %u), (%u, %u), (%u, %u), (%u, %u), (%u, %u), (%u, %u), (%u, %u), (%u, %u), %u\n",
                window->normal_hints.flags, window->normal_hints.x, window->normal_hints.y,
                window->normal_hints.width, window->normal_hints.height,
                window->normal_hints.min_width, window->normal_hints.min_height,
                window->normal_hints.max_width, window->normal_hints.max_height,
                window->normal_hints.width_inc, window->normal_hints.height_inc,
                window->normal_hints.min_aspect_num, window->normal_hints.min_aspect_den,
                window->normal_hints.max_aspect_num, window->normal_hints.max_aspect_den,
                window->normal_hints.base_width, window->normal_hints.base_height,
                window->normal_hints.win_gravity);

        set_initial_size(window);

        notef("configuring window %#x to "
                    "%" PRIi32 ", %" PRIi32 ", %" PRIi32 ", %" PRIi32 "\n",
                window->id, window->x, window->y, window->width, window->height);
        const uint16_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
        const uint32_t values[] = {
            window->x, window->y, window->width, window->height
        };
        xcb_configure_window(display.xcb, window->id, mask, values);

        window->state = XCB_ICCCM_WM_STATE_NORMAL;
    } else if (window->state == XCB_ICCCM_WM_STATE_WITHDRAWN) {
        if ((window->hints.flags & XCB_ICCCM_WM_HINT_STATE)) {
            window->state = window->hints.initial_state;
        } else {
            window->state = XCB_ICCCM_WM_STATE_NORMAL;
        }
    } else if (window->state == XCB_ICCCM_WM_STATE_ICONIC) {
        window->state = XCB_ICCCM_WM_STATE_NORMAL;
    }

    if (window->state == XCB_ICCCM_WM_STATE_NORMAL) {
        is_visible = true;
    } else {
        is_visible = false;
    }

    is_workspace_visible = add_window_to_workspace(WORKSPACE_NONE, window->id);
    if (is_visible && is_workspace_visible) {
        xcb_map_window(display.xcb, window->id);
        /* TODO: do not focus if not wanted per configuration */
        (void) focus_window(window);
    }

    if (window->state == XCB_ICCCM_WM_STATE_ICONIC) {
        /* TODO: the window goes into iconic mode */
    }

    xcb_flush(display.xcb);
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

/* Try to focus a window that makes sense or the root if none available.
 *
 * @return 0 if a top-level window got focused, otherwise non-zero and the root
 *         is focused.
 */
static int focus_next_available_window(void)
{
    /* TODO: this needs to be smarter, it just tries window recency which can
     * occur to the user as rather random
     */
    for (size_t i = windows_length; i > 0; ) {
        i--;
        if (windows[i].state == XCB_ICCCM_WM_STATE_NORMAL) {
            if (focus_window(&windows[i]) == 0) {
                return 0;
            }
        }
    }
    /* `FOCUS_NONE` for full manual management */
    xcb_set_input_focus(display.xcb, XCB_INPUT_FOCUS_NONE,
            display.root, display.last_timestamp);
    return 1;
}

/* Configure the size of a window. */
void configure_window(xcb_configure_notify_event_t *event)
{
    struct window_cache *window;

    window = get_window_by_id(event->window);
    if (event->x != window->x || event->y != window->y ||
            event->width != window->width || event->height != window->height) {
        window->x = event->x;
        window->y = event->y;
        window->width = event->width;
        window->height = event->height;
        notef("window position and size of %" PRIu32 " changed\n", window->id);
        const struct rectangle rectangle = {
            window->x, window->y, window->width, window->height
        };
        report_window_movement_to_workspaces(window->id, &rectangle);
    }
}

/* Unregister a window. */
void destroy_window(xcb_destroy_notify_event_t *event)
{
    struct window_cache *window;

    remove_window_from_workspace(event->window);

    window = get_window_by_id(event->window);
    if (window != &null_window) {
        windows_length--;
        const size_t index = window - windows;
        MOVE(window, window + 1, windows_length - index);
    }

    if (event->window == display.focus) {
        (void) focus_next_available_window();
    }

    notef("window %#x destruction registered\n", event->window);
}
