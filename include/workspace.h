#ifndef WORKSPACE_H
#define WORKSPACE_H

#include <utility/list.h>

#include "monitor.h"
#include "window.h"

/* Workspace/desktop and tiling management. */

/* a unique workspace ID */
typedef uint32_t workspace_t;

/* ID for a non existent workspace */
#define WORKSPACE_NONE 0

/* the first valid workspace ID */
#define WORKSPACE_FIRST 1

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

struct workspace {
    /* unique identifier of the workspace */
    workspace_t id;
    /* the crtc this workspace is on */
    xcb_randr_crtc_t crtc;
    /* the state the workspace is in */
    enum workspace_state state;
    /* a list of windows associated to this workspace */
    LIST(struct window*, windows);
};

/* Add a new window to a workspace.
 *
 * @window is the newly managed window.
 */
void add_window_to_workspace(struct window *window);

/* Remove the window from its current workspace. */
void remove_window_from_workspace(struct window *window);

/* Focus the workspace identified by id @workspace.
 *
 * At least one workspace MUST exist already.
 */
void focus_workspace(workspace_t workspace);

/* Notify the workspace module that the window got a map request. */
void relay_map_request_to_workspaces(struct window *window);

/* Notify the workspace module that a window has moved. */
void report_window_movement_to_workspaces(struct window *window);

/* Notify the workspace module that a monitor has changed.
 *
 * If `new_rectangle->width` is 0, this means the crtc is now disabled.
 * It might get re-enabled later.  In this case the width will be non zero.
 */
void report_monitor_change_to_workspaces(xcb_randr_crtc_t crtc,
        const struct rectangle *old_rectangle, const struct rectangle *new_rectangle);

/* Notify the workspace module that the focus has changed. */
void report_focus_change_to_workspaces(struct window *window);

#endif
