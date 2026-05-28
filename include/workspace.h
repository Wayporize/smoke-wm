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
    LIST(xcb_window_t, windows);
};

/* Add a new window to a workspace.
 *
 * @workspace is the workspace to show the window on.
 * @window is the newly managed window.
 *
 * @return if the workspace the window is added to is visible.
 */
bool add_window_to_workspace(workspace_t workspace, xcb_window_t window);

/* Remove the window from its current workspace. */
void remove_window_from_workspace(xcb_window_t window);

/* Focus the workspace identified by id @workspace.
 *
 * At least one workspace MUST exist already.
 */
void focus_workspace(workspace_t workspace);

/* Notify the workspace module that a window has moved. */
void report_window_movement_to_workspaces(xcb_window_t window, const struct rectangle *rectangle);

/* Notify the workspace module that a monitor has changed.
 *
 * If `new_rectangle->width` is 0, this means the crtc is now disabled.
 * It might get re-enabled later.  In this case the width will be non zero.
 */
void report_monitor_change_to_workspaces(xcb_randr_crtc_t crtc,
        const struct rectangle *old_rectangle, const struct rectangle *new_rectangle);

/* Notify the workspace module that the focus has changed. */
void report_focus_change_to_workspaces(xcb_window_t window);

#endif
