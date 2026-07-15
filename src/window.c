#include <inttypes.h>
#include <utility/list.h>
#include <utility/log.h>
#include <utility/utility.h>

#include <xcb/xcb_icccm.h>

#include "configuration.h"
#include "display.h"
#include "monitor.h"
#include "window.h"

/* list of all windows */
STATIC_LIST(struct window*, windows);

/* Get the internal representation of an X window. */
struct window *get_internal_window(xcb_window_t id)
{
    for (size_t i = 0; i < windows_length; i++) {
        if (windows[i]->id == id) {
            return windows[i];
        }
    }
    return NULL;
}

/* Create and register a new window from an X11 event. */
void create_window(xcb_create_notify_event_t *event)
{
    const uint32_t values[] = {
        XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_FOCUS_CHANGE
    };
    xcb_change_window_attributes(display.xcb, event->window, XCB_CW_EVENT_MASK, values);

    const struct window window_data = {
        .id = event->window, .x = event->x, .y = event->y, .width = event->width, .height = event->height,
        .state = XCB_ICCCM_WM_STATE_NEW
    };
    struct window *const window = DUPLICATE(&window_data, 1);
    LIST_APPEND_VALUE(windows, window);

    notef("window %#x creation registered\n", event->window);
}

/* Notify of a property change in a window. */
void change_property(xcb_property_notify_event_t *event)
{
    struct window *const window = get_internal_window(event->window);
    if (window == NULL) {
        return;
    }
    /* TODO: react to name changes and what not */
}

/* Set an initial size using given size hints. */
static void set_initial_size(struct window *window)
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
static int focus_window(struct window *window)
{
    /* check if the window needs us to focus it directly */
    if (((window->hints.flags & XCB_ICCCM_WM_HINT_INPUT) && window->hints.input) ||
                /* assume input = true if missing */
                !(window->hints.flags & XCB_ICCCM_WM_HINT_INPUT)) {
        /* `FOCUS_NONE` for full manual management */
        xcb_set_input_focus(display.xcb, XCB_INPUT_FOCUS_NONE, window->id, display.last_timestamp);
        return 0;
    /* send a client message if the `WM_TAKE_FOCUS` protocol is supported */
    } else if (window->protocols.has_wm_take_focus) {
        xcb_client_message_event_t message;

        message.response_type = XCB_CLIENT_MESSAGE;
        message.window = window->id;
        message.format = 32;
        message.type = display.wm_protocols;
        message.data.data32[0] = display.wm_take_focus;
        message.data.data32[1] = display.last_timestamp;
        xcb_send_event(display.xcb, false, window->id, XCB_EVENT_MASK_NO_EVENT, (char*) &message);
        return 0;
    }
    return 1;
}

/* Start to actually manage the window. */
void manage_new_window(struct window *window, struct wm_window *configuration)
{
    xcb_get_property_cookie_t hints_cookie, normal_hints_cookie, protocols_cookie,
                              name_cookie, class_cookie;
    xcb_icccm_get_wm_protocols_reply_t protocols;
    xcb_icccm_get_text_property_reply_t text;
    xcb_icccm_get_wm_class_reply_t class;

    hints_cookie = xcb_icccm_get_wm_hints(display.xcb, window->id);
    normal_hints_cookie = xcb_icccm_get_wm_normal_hints(display.xcb, window->id);
    protocols_cookie = xcb_icccm_get_wm_protocols(display.xcb, window->id, display.wm_protocols);
    name_cookie = xcb_icccm_get_wm_name(display.xcb, window->id);
    class_cookie = xcb_icccm_get_wm_class(display.xcb, window->id);

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
    (void) xcb_icccm_get_wm_name_reply(display.xcb, name_cookie, &text, NULL);
    /* TODO: what about the encoding and format? */
    window->name = xstrndup(text.name, text.name_len);
    xcb_icccm_get_text_property_reply_wipe(&text);

    (void) xcb_icccm_get_wm_class_reply(display.xcb, class_cookie, &class, NULL);
    window->instance = xstrdup(class.instance_name);
    window->class = xstrdup(class.class_name);
    xcb_icccm_get_wm_class_reply_wipe(&class);

    get_window_configuration(window, configuration);

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

    xcb_flush(display.xcb);
}

/* Handle when a client wants to map (show) a window. */
void handle_map_request(xcb_map_request_event_t *event)
{
    notef("got map request for %#x\n", event->window);

    struct window *const window = get_internal_window(event->window);
    if (window == NULL) {
        return;
    }

    /* check if the window is mapped for the first time */
    if (window->state == XCB_ICCCM_WM_STATE_NEW) {
        struct wm_window configuration;

        manage_new_window(window, &configuration);

        if ((window->hints.flags & XCB_ICCCM_WM_HINT_STATE)) {
            window->state = window->hints.initial_state;
        } else {
            window->state = XCB_ICCCM_WM_STATE_NORMAL;
        }

        add_window_to_workspace(configuration.workspace, window);
    } else {
        /* transition the state in compliance with ICCCM */
        if (window->state == XCB_ICCCM_WM_STATE_WITHDRAWN) {
            if ((window->hints.flags & XCB_ICCCM_WM_HINT_STATE)) {
                window->state = window->hints.initial_state;
            } else {
                window->state = XCB_ICCCM_WM_STATE_NORMAL;
            }
        } else if (window->state == XCB_ICCCM_WM_STATE_ICONIC) {
            window->state = XCB_ICCCM_WM_STATE_NORMAL;
        }
    }

    /* overrule the window state according to the workspace state */
    struct workspace *const workspace = get_window_workspace(window);
    if (workspace != NULL && workspace->state == WORKSPACE_HIDDEN) {
        window->state = XCB_ICCCM_WM_STATE_ICONIC;
    }

    if (window->state == XCB_ICCCM_WM_STATE_NORMAL) {
        notef("window %#" PRIx32 " is shown\n", window->id);
        xcb_map_window(display.xcb, window->id);
        /* TODO: configure if a window should be auto focused? */
        (void) focus_window(window);
    }

    /* TODO: the second value is the window id of the icon */
    const xcb_atom_t atoms[] = { window->state, XCB_NONE };
    xcb_change_property(display.xcb, XCB_PROP_MODE_REPLACE, window->id,
            display.wm_state, display.wm_state, 32, 2, (const void*) atoms);
}

/* Handle when a client wants to change the geometry or stacking of a window. */
void handle_configure_request(xcb_configure_request_event_t *event)
{
    xcb_configure_notify_event_t *synthetic_event;

    struct window *const window = get_internal_window(event->window);
    if (window == NULL) {
        return;
    }

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
        if (windows[i]->state == XCB_ICCCM_WM_STATE_NORMAL) {
            if (focus_window(windows[i]) == 0) {
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
    struct window *const window = get_internal_window(event->window);
    if (window != NULL && (event->x != window->x || event->y != window->y ||
                event->width != window->width || event->height != window->height)) {
        struct workspace *workspace;

        window->x = event->x;
        window->y = event->y;
        window->width = event->width;
        window->height = event->height;
        notef("window position and size of %#" PRIx32 " changed\n", window->id);

        /* change the workspace based on the new position */
        if (window->state != XCB_ICCCM_WM_STATE_NORMAL) {
            /* ignore invisible windows */
            return;
        }
        workspace = get_window_workspace(window);
        if (workspace == NULL) {
            /* do not add it to a workspace if it was not associated to any */
            return;
        }

        struct monitor *const monitor = get_monitor_from_rectangle(window->x, window->y, window->width, window->height);
        if (monitor == NULL) {
            return;
        }

        /* get the active workspace */
        struct workspace *active_workspace = NULL;
        for (size_t i = 0; i < monitor->workspaces_length; i++) {
            if (monitor->workspaces[i] == workspace) {
                /* the window is already on the workspace it is supposed to be
                 * on
                 */
                return;
            }
            if (monitor->workspaces[i]->state >= WORKSPACE_VISIBLE) {
                active_workspace = monitor->workspaces[i];
            }
        }

        add_window_to_workspace(active_workspace->name, window);
    }
}

/* Unregister a window. */
void destroy_window(xcb_destroy_notify_event_t *event)
{
    struct window *const window = get_internal_window(event->window);
    if (window == NULL) {
        return;
    }

    remove_window_from_workspace(window);

    for (size_t i = 0; i < windows_length; i++) {
        if (windows[i] == window) {
            LIST_REMOVE(windows, i, 1);
            break;
        }
    }

    if (event->window == display.focus) {
        (void) focus_next_available_window();
    }

    notef("window %#x destruction registered\n", event->window);
}

/* Get the position of the window relative to the workspace/output it is on. */
static void get_relative_window_position(struct window *window, int32_t *x, int32_t *y)
{
    struct workspace *workspace;
    struct monitor *monitor;

    workspace = get_window_workspace(window);
    if (workspace != NULL) {
        monitor = workspace->monitor;
    } else {
        monitor = get_monitor_from_rectangle(window->x, window->y, window->width, window->height);
    }

    if (monitor == NULL) {
        *x = window->x;
        *y = window->y;
    } else {
        *x = window->x - monitor->x;
        *y = window->y - monitor->y;
    }
}

/* Notify the window module that the configuration has changed. */
void report_configuration_change_to_windows(struct wm_window *configured, size_t configured_length)
{
    for (size_t i = 0; i < configured_length; i++) {
        /* move the matching windows to their configured workspace/output */
        for (size_t j = 0; j < windows_length; j++) {
            if ((configured[i].name == NULL || matches_pattern(configured[i].name, windows[j]->name)) &&
                    (configured[i].class == NULL || matches_pattern(configured[i].class, windows[j]->class)) &&
                    (configured[i].instance == NULL || matches_pattern(configured[i].instance, windows[j]->instance))) {
                if (configured[i].workspace != NULL) {
                    add_window_to_workspace(configured[i].workspace, windows[j]);
                } else if (configured[i].output != NULL) {
                    int32_t x, y;
                    struct monitor *const monitor = get_monitor_from_output_name(configured[i].output);
                    get_relative_window_position(windows[j], &x, &y);
                    remove_window_from_workspace(windows[j]);
                    windows[j]->x = monitor->x + x;
                    windows[j]->y = monitor->y + y;
                    const uint16_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y;
                    const uint32_t values[] = { windows[j]->x, windows[j]->y };
                    xcb_configure_window(display.xcb, windows[j]->id, mask, values);
                }
            }
        }
    }
}
