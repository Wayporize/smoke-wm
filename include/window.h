#ifndef WINDOW_H
#define WINDOW_H

#include <xcb/xcb_icccm.h>

#include "workspace.h"

/* property pair */
struct window_property {
    /* outgoing request to the server or not outgoing if `sequence` is 0 */
    xcb_get_property_cookie_t cookie;
    /* reply received from the request or NULL if no reply received yet */
    xcb_get_property_reply_t *reply;
};

/* cache for properties and geometry of an X11 window */
struct window_cache {
    /* the X11 window id */
    xcb_window_t id;
    /* position and size of the window */
    int32_t x, y, width, height;
    /* size hints set by a client to properly size the window */
    struct window_property wm_normal_hints;
    /* additional hints set by a client */
    struct window_property wm_hints;
    /* the current window state */
    xcb_icccm_wm_state_t state;
    /* the workspace this window is on */
    workspace_t workspace;
};

/* TODO: Go through all windows that already exist and manage them. */
void query_existing_windows(void);

/* Create and register a new window from an X11 event. */
void create_window(xcb_create_notify_event_t *event);

/* Change a property of a window.
 *
 * The new property value is just queued for retrieval but no roundtrip to the
 * server is issued to actually get the value, see `update_property()`.
 */
void change_property(xcb_property_notify_event_t *event);

/* Update a specific window property.
 *
 * Call this before using any of the `reply` values.
 *
 * @return 1 if the property does not exist on this window, 0 otherwise.
 */
int update_property(struct window_property *property);

/* Handle when a client wants to map (show) a window. */
void handle_map_request(xcb_map_request_event_t *event);

/* Handle when a client wants to change the geometry or stacking of a window. */
void handle_configure_request(xcb_configure_request_event_t *event);

/* Tell the window module the new focused window. */
void report_focus_change(xcb_window_t window);

/* Unregister a window. */
void destroy_window(xcb_destroy_notify_event_t *event);

#endif
