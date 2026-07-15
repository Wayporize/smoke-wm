#ifndef MONITOR_H
#define MONITOR_H

#include <utility/attributes.h>
#include <utility/types.h>

#include <xcb/randr.h>

/* Screen/Monitor management using the XRandr extension (not Xinerama because
 * it is old and dumb).
 */

/* + Workspace/desktop and tiling management (Should be separated in the future). */

enum workspace_state {
    /* the workspace cannot be seen */
    WORKSPACE_HIDDEN,
    /* the workspace is visible */
    WORKSPACE_VISIBLE,
    /* there is exactly one workspace at any given time with this state, this
     * value also implies that the workspace is visible
     */
    WORKSPACE_ACTIVE,
};

struct window;
struct monitor;
struct workspace {
    /* name and unique identifier of this workspace */
    utf8_t *name;
    /* the state the workspace is in */
    enum workspace_state state;
    /* a list of windows associated to this workspace */
    LIST(struct window*, windows);
    /* the monitor this workspace is on */
    struct monitor *monitor;
};

/* physical output device */
struct output {
    /* identifier of this output */
    xcb_randr_output_t id;
    /* UTF-8 encoded name of this output device */
    utf8_t *name;
    /* crtc projected onto this output */
    xcb_randr_crtc_t crtc;
    /* connection status of this output */
    xcb_randr_connection_t connection;
};

/* each monitor corresponds to a CRTC (rectangular subsection of the screen) */
struct monitor {
    /* the crtc identifier */
    xcb_randr_crtc_t id;
    /* the crtc is disabled if this is `XCB_NONE` */
    xcb_randr_mode_t mode;
    /* position and size of the monitor (CRTC) */
    int32_t x, y, width, height;
    /* rotation of the crtc */
    xcb_randr_rotation_t rotation;
    /* the workspaces on this monitor */
    LIST(struct workspace*, workspaces);
};

/* Initialize the output and monitor list with the current RandR configuration. */
void initialize_monitor_setup(xcb_randr_get_screen_resources_cookie_t cookie);

/* Get the monitor that is projected onto the output with given name. */
struct monitor *get_monitor_from_output_name(const utf8_t *name);

/* Get the monitor that intersects given rectangle most.
 *
 * If the mid point of the rectangle is contained in any monitor, this will have
 * priority over the interection area.
 */
struct monitor *get_monitor_from_rectangle(int32_t x, int32_t y, int32_t width, int32_t height);

/* Cache output properties. */
void change_output(xcb_randr_output_t output, xcb_randr_crtc_t crtc,
        xcb_randr_connection_t connection, xcb_timestamp_t config_timestamp);

/* Cache crtc properties. */
void change_crtc(xcb_randr_crtc_t crtc, xcb_randr_mode_t mode, xcb_randr_rotation_t rotation,
        int32_t x, int32_t y, int32_t width, int32_t height);

/* Add a window to a workspace.
 *
 * @name may be `NULL` in which case the window is added to the active
 *       workspace.
 * @window is the newly managed window.
 */
void add_window_to_workspace(const utf8_t *name, struct window *window);

/* Remove the window from its current workspace. */
void remove_window_from_workspace(struct window *window);

/* Get the workspace a window is on. */
struct workspace *get_window_workspace(struct window *window);

/* Focus the workspace identified by id @name.
 *
 * At least one workspace MUST exist already.
 */
void focus_workspace(const utf8_t *name);

/* Notify the workspace module that the focus has changed. */
void report_focus_change_to_workspaces(struct window *window);

struct wm_workspace;
/* Notify the workspace module that the configuration has changed.
 *
 * @configured        are the new configuration entries.
 * @configured_length is the number of new configuration entries.
 */
void report_configuration_change_to_workspaces(struct wm_workspace *configured, size_t configured_length);

#endif
