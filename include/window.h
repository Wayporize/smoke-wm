#ifndef WINDOW_H
#define WINDOW_H

#include <utility/types.h>

#include <xcb/xcb_icccm.h>

/* another state for `xcb_icccm_wm_state_t` to indicate the window is new */
#define XCB_ICCCM_WM_STATE_NEW ((xcb_icccm_wm_state_t) 4)

/* cache for properties and geometry of an X11 window */
struct window_cache {
    /* the X11 window id */
    xcb_window_t id;
    /* position and size of the window */
    int32_t x, y, width, height;
    /* additional hints set by a client */
    xcb_icccm_wm_hints_t hints;
    /* size hints set by a client to properly size the window */
    xcb_size_hints_t normal_hints;
    /* supported ICCCM protocols by the client for this window */
    struct wm_protocols {
        /* if the `WM_TAKE_FOCUS` client message can be used */
        bool has_wm_take_focus;
    } protocols;
    /* the current window state */
    xcb_icccm_wm_state_t state;
};

/* TODO: Go through all windows that already exist and manage them. */
void query_existing_windows(void);

/* Create and register a new window from an X11 event. */
void create_window(xcb_create_notify_event_t *event);

/* Get the cached position and size of given X window. */
void get_window_rectangle(xcb_window_t window, struct rectangle *rectangle);

/* Change a property of a window.
 *
 * The new property value is just queued for retrieval but no roundtrip to the
 * server is issued to actually get the value, see `update_property()`.
 */
void change_property(xcb_property_notify_event_t *event);

/* Handle when a client wants to map (show) a window. */
void handle_map_request(xcb_map_request_event_t *event);

/* Handle when a client wants to change the geometry or stacking of a window. */
void handle_configure_request(xcb_configure_request_event_t *event);

/* Configure the size of a window, this only affects the internal state. */
void configure_window(xcb_configure_notify_event_t *event);

/* Unregister a window. */
void destroy_window(xcb_destroy_notify_event_t *event);

#endif
