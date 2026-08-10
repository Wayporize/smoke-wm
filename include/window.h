#ifndef WINDOW_H
#define WINDOW_H

#include <utility/types.h>

#include <xcb/xcb_icccm.h>

/* another state for `xcb_icccm_wm_state_t` to indicate the window is new */
#define XCB_ICCCM_WM_STATE_NEW ((xcb_icccm_wm_state_t) 4)

struct workspace;
/* cache for properties and geometry of an X11 window */
struct window {
    /* The outer window id and the inner window id.
     * If a window has a frame, `id` will the be original window and `outer_id`
     * the frame id.  If the window has no frame, `id` is the window id and
     * `outer_id` is the exact same. */
    xcb_window_t id, outer_id;
    /* graphics context for painting on window frames */
    xcb_gcontext_t gc;
    /* position and size of the window */
    int32_t x, y, width, height;
    /* the current window state */
    xcb_icccm_wm_state_t state;
    /* additional hints set by a client */
    xcb_icccm_wm_hints_t hints;
    /* size hints set by a client to properly size the window */
    xcb_size_hints_t normal_hints;
    /* supported ICCCM protocols by the client for this window */
    struct wm_protocols {
        /* if the `WM_TAKE_FOCUS` client message can be used */
        bool has_wm_take_focus;
    } protocols;
    /* focus number: the higher the number, the more recent the focus */
    uint64_t focus_order;
    /* window text properties */
    utf8_t *name, *instance, *class;
};

/* TODO: Go through all windows that already exist and manage them. */
void query_existing_windows(void);

/* Get the internal representation of an X window. */
struct window *get_internal_window(xcb_window_t window);

/* Create and register a new window from an X11 event. */
void create_window(xcb_create_notify_event_t *event);

/* Redraw the frame of a window. */
void redraw_window(xcb_window_t id);

/* Change a property of a window.
 *
 * The new property value is just queued for retrieval but no roundtrip to the
 * server is issued to actually get the value, see `update_property()`.
 */
void change_property(xcb_property_notify_event_t *event);

/* If the window can receive focus. */
bool is_focusable(struct window *window);

/* Focus a specific window in the X world. */
void focus_window(struct window *window);

/* Grab a button on every managed window. */
void grab_button_on_all_windows(uint16_t event_mask, uint8_t button, uint16_t modifiers);

/* Ungrab a button on every managed window. */
void ungrab_button_on_all_windows(uint8_t button, uint16_t modifiers);

/* Try to focus a window that makes sense or the root if none available. */
void focus_next_available_window(void);

/* Handle when a client wants to map (show) a window. */
void handle_map_request(xcb_map_request_event_t *event);

/* Update the focus number of @window to be the most recent. */
void update_window_focus_number(struct window *window);

/* Update the internal window focus to a new window.
 *
 * @window may be `NULL` in which case the root is the new focus.
 */
void update_window_focus(struct window *window);

/* Try to focus a window that makes sense or the root if none available. */
void focus_next_available_window(void);

/* Handle when a client wants to map (show) a window. */
void handle_map_request(xcb_map_request_event_t *event);

/* Handle when a client wants to change the geometry or stacking of a window. */
void handle_configure_request(xcb_configure_request_event_t *event);

/* Configure the size of a window, this only affects the internal state. */
void configure_window(xcb_configure_notify_event_t *event);

/* Update the state of a window. */
void change_window_state(xcb_window_t window, xcb_icccm_wm_state_t state);

/* Close a specific window. */
void close_window(xcb_window_t window);

/* Move a window to a different workspace/output. */
void move_window(xcb_window_t window, const utf8_t *destination);

/* Unregister a window. */
void destroy_window(xcb_destroy_notify_event_t *event);

struct wm_window;
/* Notify the window module that the configuration has changed.
 *
 * @configured        are the new configuration entries.
 * @configured_length is the number of new configuration entries.
 */
void report_configuration_change_to_windows(struct wm_window *configured, size_t configured_length);

#endif
