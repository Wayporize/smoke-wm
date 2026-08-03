#include <inttypes.h>
#include <utility/list.h>
#include <utility/log.h>
#include <utility/utility.h>

#include <xcb/xcb_icccm.h>

#include "binding.h"
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
        if (windows[i]->id == id || windows[i]->frame == id) {
            return windows[i];
        }
    }
    return NULL;
}

/* Create and register a new window from an X11 event. */
void create_window(xcb_create_notify_event_t *event)
{
    struct window *window = get_internal_window(event->window);
    if (window != NULL) {
        /* do not register the frame windows we created ourselves */
        return;
    }

    const uint32_t values[] = {
        XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_FOCUS_CHANGE
    };
    xcb_change_window_attributes(display.xcb, event->window, XCB_CW_EVENT_MASK, values);

    const struct window window_data = {
        .id = event->window, .x = event->x, .y = event->y, .width = event->width, .height = event->height,
        .state = XCB_ICCCM_WM_STATE_NEW
    };
    window = DUPLICATE(&window_data, 1);
    LIST_APPEND_VALUE(windows, window);

    LOG("window %#x creation registered\n", event->window);
}

/* Setup the frame of a window. */
static void setup_window_frame(struct window *window, enum border_decoration decoration, int32_t border_size)
{
    if (decoration == BORDER_NONE) {
        if (window->frame == XCB_NONE) {
            /* the window already has no border */
            return;
        }
        xcb_free_gc(display.xcb, window->gc);
        xcb_destroy_window(display.xcb, window->frame);
        window->frame = XCB_NONE;
        /* give the window its size with the border deducted */
        const uint16_t configure_mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
        const uint32_t configure_values[] = {
            window->x, window->y, window->width, window->height
        };
        xcb_configure_window(display.xcb, window->id, configure_mask, configure_values);
        LOG("configuring window %#" PRIx32 " to %#" PRId32 ", %#" PRId32 ", %#" PRId32 ", %#" PRId32 "\n",
                window->id, window->x, window->y, window->width, window->height);
    } else {
        if (window->frame != XCB_NONE) {
            /* the window already has a border */
            return;
        }
        /* create the actual outer frame window */
        const uint32_t mask = XCB_CW_EVENT_MASK;
        const uint32_t values[] = {
            XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_STRUCTURE_NOTIFY |
                XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_FOCUS_CHANGE };
        window->frame = xcb_generate_id(display.xcb);
        xcb_create_window(display.xcb, XCB_COPY_FROM_PARENT, window->frame, display.root,
                window->x, window->y, window->width, window->height, 0,
                XCB_WINDOW_CLASS_COPY_FROM_PARENT, XCB_COPY_FROM_PARENT, mask, values);

        /* create a graphics context to render the border with */
        window->gc = xcb_generate_id(display.xcb);
        xcb_create_gc(display.xcb, window->gc, window->frame, 0, NULL);

        /* add the window to the save set such that if the window manager exits, the
         * window is automatically reparented to the root again */
        xcb_change_save_set(display.xcb, XCB_SET_MODE_INSERT, window->id);

        /* make the new window a child of the frame window */
        LOG("reparenting window into frame %#" PRIx32 "\n", window->id);
        xcb_reparent_window(display.xcb, window->id, window->frame, border_size, border_size);

        /* map the inner window, it will not actually be shown */
        xcb_map_window(display.xcb, window->id);
    }
}

/* Redraw the frame of a window. */
void redraw_window(xcb_window_t id)
{
    struct window *const window = get_internal_window(id);
    if (window == NULL || window->frame == XCB_NONE) {
        return;
    }
    struct wm_window configuration;
    (void) get_window_configuration(window, &configuration);
    const xcb_rectangle_t rectangle = { window->x, window->y, window->width, window->height };
    const uint32_t mask = XCB_GC_FOREGROUND;
    struct wm_color color;
    if (window->id == display.focus) {
        color = configuration.border.color.focused;
    } else {
        color = configuration.border.color.inactive;
    }
    const uint32_t values[] = { ((color.red / 256) << 16) | ((color.green / 256) << 8) | (color.blue / 256) };
    xcb_change_gc(display.xcb, window->gc, mask, values);
    xcb_poly_fill_rectangle(display.xcb, window->frame, window->gc, 1, &rectangle);
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

/* If the window can receive focus. */
bool is_focusable(struct window *window)
{
    if (window == NULL) {
        /* `NULL` represents the root window */
        return true;
    }

    if (window->state != XCB_ICCCM_WM_STATE_NORMAL) {
        /* can not focus invisible windows */
        return false;
    }

    if (((window->hints.flags & XCB_ICCCM_WM_HINT_INPUT) && window->hints.input) ||
                /* assume input = true if missing */
                !(window->hints.flags & XCB_ICCCM_WM_HINT_INPUT)) {
        return true;
    }

    if (window->protocols.has_wm_take_focus) {
        return true;
    }
    return false;
}

/* Show a window in the X world. */
void show_window(struct window *window)
{
    if (window->frame != XCB_NONE) {
        LOG("window %#" PRIx32 "(%#" PRIx32 ") is shown\n", window->frame, window->id);
        xcb_map_window(display.xcb, window->frame);
    } else {
        LOG("window %#" PRIx32 " is shown\n", window->id);
        xcb_map_window(display.xcb, window->id);
    }
}

/* Hide a window in the X world. */
void hide_window(struct window *window)
{
    if (window->frame != XCB_NONE) {
        LOG("window %#" PRIx32 "(%#" PRIx32 ") is hidden\n", window->frame, window->id);
        xcb_unmap_window(display.xcb, window->frame);
    } else {
        LOG("window %#" PRIx32 " is hidden\n", window->id);
        xcb_unmap_window(display.xcb, window->id);
    }
}

/* Focus a specific window in the X world. */
void focus_window(struct window *window)
{
    if (window == NULL) {
        /* `FOCUS_NONE` for full manual management */
        xcb_set_input_focus(display.xcb, XCB_INPUT_FOCUS_NONE,
                display.root, display.last_timestamp);
    /* check if the window needs us to focus it directly */
    } else if (((window->hints.flags & XCB_ICCCM_WM_HINT_INPUT) && window->hints.input) ||
                /* assume input = true if missing */
                !(window->hints.flags & XCB_ICCCM_WM_HINT_INPUT)) {
        /* `FOCUS_NONE` for full manual management */
        xcb_set_input_focus(display.xcb, XCB_INPUT_FOCUS_NONE, window->id, display.last_timestamp);
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
    }
}

/* Grab a button on every managed window. */
void grab_button_on_all_windows(uint16_t event_mask, uint8_t button, uint16_t modifiers)
{
    for (size_t i = 0; i < windows_length; i++) {
        xcb_grab_button(display.xcb, true, windows[i]->id, event_mask, XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC, XCB_NONE, XCB_NONE, button, modifiers);
    }
}

/* Ungrab a button on every managed window. */
void ungrab_button_on_all_windows(uint8_t button, uint16_t modifiers)
{
    for (size_t i = 0; i < windows_length; i++) {
        xcb_ungrab_button(display.xcb, button, windows[i]->id, modifiers);
    }
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
                LOG("window supports WM_TAKE_FOCUS\n");
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

    /* get the window configuration */
    get_window_configuration(window, configuration);

    /* get some sensible initial floating size */
    set_initial_size(window);

    /* create the outer frame window if the configuration requires it */
    setup_window_frame(window, configuration->border.decoration, configuration->border.size);

    grab_transparent_button_bindings_for_window(window->id);

    /* make all of this immediately noted to the server */
    xcb_flush(display.xcb);
}

/* Handle when a client wants to map (show) a window. */
void handle_map_request(xcb_map_request_event_t *event)
{
    LOG("got map request for %#x\n", event->window);

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
        show_window(window);
        /* TODO: configure if a window should be auto focused? */
        focus_window(window);
    }

    /* TODO: the second value is the window id of the icon */
    const xcb_atom_t atoms[] = { window->state, XCB_NONE };
    xcb_change_property(display.xcb, XCB_PROP_MODE_REPLACE, window->id,
            display.wm_state, display.wm_state, 32, 2, (const void*) atoms);
}

/* Update the focus number of @window to be the most recent. */
void update_window_focus(struct window *window)
{
    uint64_t maximum_focus = 0;

    for (size_t i = 0; i < windows_length; i++) {
        if (windows[i] == window) {
            continue;
        }
        if (maximum_focus < windows[i]->focus_order) {
            maximum_focus = windows[i]->focus_order;
        }
    }

    /* make sure the window has the biggest focus of all */
    window->focus_order = maximum_focus + 1;
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

    LOG("sending synthetic configure event to %#" PRIx32 "\n", event->window);
    xcb_send_event(display.xcb, false, event->window,
            XCB_EVENT_MASK_STRUCTURE_NOTIFY, (char*) synthetic_event);
    free(synthetic_event);
    xcb_flush(display.xcb);
}

/* Try to focus a window that makes sense or the root if none available.
 *
 * @window is the window the focus should be relative to. This window will
 *         itself not be focused.
 */
static void focus_next_available_window(struct window *window)
{
    struct window **search_windows;
    size_t search_windows_length;
    struct workspace *workspace = get_window_workspace(window);
    if (workspace == NULL) {
        struct monitor *monitor = get_monitor_from_rectangle(window->x, window->y, window->width, window->height);
        if (monitor == NULL) {
            search_windows = windows;
            search_windows_length = windows_length;
        } else {
            for (size_t i = 0; i < monitor->workspaces_length; i++) {
                if (monitor->workspaces[i]->state >= WORKSPACE_VISIBLE) {
                    workspace = monitor->workspaces[i];
                    break;
                }
            }
        }
    }
    /* do not use an else */
    if (workspace != NULL) {
        search_windows = workspace->windows;
        search_windows_length = workspace->windows_length;
    }

    uint64_t maximum_focus = 0;
    struct window *candidate_window = NULL;
    for (size_t i = 0; i < search_windows_length; i++) {
        if (search_windows[i] == window) {
            /* ignore the previously focused window */
            continue;
        }
        if (search_windows[i]->state != XCB_ICCCM_WM_STATE_NORMAL) {
            /* ignore invisible windows */
            continue;
        }
        /* prefer the window if it was focused more recently */
        if (search_windows[i]->focus_order >= maximum_focus) {
            maximum_focus = search_windows[i]->focus_order;
            candidate_window = search_windows[i];
        }
    }
    focus_window(candidate_window);
}

/* Configure the size of a window. */
void configure_window(xcb_configure_notify_event_t *event)
{
    struct window *const window = get_internal_window(event->window);
    /* only consider configure events for frames or windows without frames */
    if (window == NULL || (window->id == event->window && window->frame != XCB_NONE)) {
        return;
    }
    /* check if the size/position changed */
    if (event->x != window->x || event->y != window->y || event->width != window->width || event->height != window->height) {
        struct workspace *workspace;
        struct wm_window configuration;

        window->x = event->x;
        window->y = event->y;
        window->width = event->width;
        window->height = event->height;

        if (window->frame != XCB_NONE) {
            LOG("window position and size of %#" PRIx32 " changed\n", window->frame);
            LOG("frame size changed, also resizing inner window %#" PRIx32 "\n", window->id);
            (void) get_window_configuration(window, &configuration);
            const uint16_t configure_mask = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
            const uint32_t configure_values[] = {
                window->width - 2 * configuration.border.size, window->height - 2 * configuration.border.size
            };
            xcb_configure_window(display.xcb, window->id, configure_mask, configure_values);
        } else {
            LOG("window position and size of %#" PRIx32 " changed\n", window->id);
        }

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
                 * on */
                return;
            }
            if (monitor->workspaces[i]->state >= WORKSPACE_VISIBLE) {
                active_workspace = monitor->workspaces[i];
            }
        }

        add_window_to_workspace(active_workspace->name, window);
    }
}

/* Update the state of a window. */
void change_window_state(xcb_window_t id, xcb_icccm_wm_state_t state)
{
    struct window *const window = get_internal_window(id);
    if (window == NULL) {
        return;
    }
    window->state = state;
    if (state != XCB_ICCCM_WM_STATE_NORMAL) {
        if (id == window->id && window->frame != XCB_NONE) {
            xcb_unmap_window(display.xcb, window->frame);
            xcb_map_window(display.xcb, window->id);
        }
        if (id == display.focus) {
            focus_next_available_window(window);
        }
    }

    /* update the `WM_STATE` property */
    const xcb_atom_t atoms[] = { window->state, XCB_NONE };
    xcb_change_property(display.xcb, XCB_PROP_MODE_REPLACE, window->id,
            display.wm_state, display.wm_state, 32, 2, (const void*) atoms);
}

/* Close a specific window. */
void close_window(xcb_window_t id)
{
    struct window *const window = get_internal_window(id);
    if (window == NULL) {
        return;
    }
    /* TODO: */
}

/* Move a window to a different workspace/output. */
void move_window(xcb_window_t id, const utf8_t *destination)
{
    struct window *const window = get_internal_window(id);
    if (window == NULL) {
        return;
    }
    if (change_window_workspace(window, destination) != 0) {
        struct monitor *const monitor = get_monitor_from_output_name(destination);
        if (monitor != NULL) {
            for (size_t i = 0; i < monitor->workspaces_length; i++) {
                if (monitor->workspaces[i]->state >= WORKSPACE_VISIBLE) {
                    change_window_workspace_directly(window, monitor->workspaces[i]);
                    break;
                }
            }
        }
    }
}

/* Unregister a window. */
void destroy_window(xcb_destroy_notify_event_t *event)
{
    struct window *const window = get_internal_window(event->window);
    if (window == NULL) {
        return;
    }

    if (window->id == display.focus) {
        focus_next_available_window(window);
    }

    remove_window_from_workspace(window);

    for (size_t i = 0; i < windows_length; i++) {
        if (windows[i] == window) {
            LIST_REMOVE(windows, i, 1);
            break;
        }
    }

    /* also destroy the surrounding frame if it exists */
    if (window->frame != XCB_NONE) {
        xcb_destroy_window(display.xcb, window->frame);
    }

    LOG("window %#x destruction registered\n", window->id);

    /* free all window resources */
    free(window->name);
    free(window->instance);
    free(window->class);
    free(window);
}

/* Get the position of the window relative to the workspace/output it is on. */
static void get_relative_window_position(struct window *window, int32_t *x, int32_t *y)
{
    struct workspace *workspace;
    struct monitor *monitor;

    workspace = get_window_workspace(window);
    if (workspace != NULL) {
        monitor = get_workspace_monitor(workspace);
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
                    /* isolate the window from its current workspace and
                     * position it relative to the configured output */
                    int32_t x, y;
                    struct monitor *const monitor = get_monitor_from_output_name(configured[i].output);
                    get_relative_window_position(windows[j], &x, &y);
                    remove_window_from_workspace(windows[j]);
                    windows[j]->x = monitor->x + x;
                    windows[j]->y = monitor->y + y;
                    const uint16_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y;
                    const uint32_t values[] = { windows[j]->x, windows[j]->y };
                    xcb_configure_window(display.xcb, windows[j]->id, mask, values);
                } else if (configured[i].border.decoration != BORDER_UNSPECIFIED) {
                    setup_window_frame(windows[j], configured[i].border.decoration, configured[i].border.size);
                }
            }
        }
    }
}
