#include <inttypes.h>
#include <utility/log.h>
#include <utility/utility.h>

#include "configuration.h"
#include "display.h"
#include "workspace.h"

/* list of all workspaces sorted by their ID, each workspace in this list has an
 * associated monitor
 */
STATIC_LIST(struct workspace, workspaces);

/* TODO: integrate tiling directly into here */

/* Get the data of a workspace. */
static struct workspace *get_workspace(workspace_t id)
{
    /* TODO: binary search */
    for (size_t i = 0; i < workspaces_length; i++) {
        if (workspaces[i].id == id) {
            return &workspaces[i];
        }
    }
    return NULL;
}

/* Add a non-existent workspace to `workspaces`. */
static struct workspace *add_workspace(workspace_t id)
{
    size_t index;
    LIST(utf8_t, strings);
    utf8_t *string;

    /* TODO: binary search */
    /* find the index the workspace fits into */
    for (index = 0; index < workspaces_length; index++) {
        if (workspaces[index].id > id) {
            break;
        }
    }

    /* make a gap for the new workspace */
    LIST_GROW(workspaces, workspaces_length + 1);
    MOVE(&workspaces[index + 1], &workspaces[index], workspaces_length - index);
    workspaces_length++;

    /* initialize the workspace */
    ZERO(&workspaces[index], 1);
    workspaces[index].id = id;

    notef("workspace %" PRIu32 " added\n", id);

    LIST_INITIALIZE(strings, 32);
    for (size_t i = 0; i < workspaces_length; i++) {
        utf8_t *name = NULL;

        /* try to find a configured name for this workspace */
        for (size_t j = 0; j < Configuration.workspace_length; j++) {
            if (Configuration.workspace[j].number == workspaces[i].id) {
                name = Configuration.workspace[j].name;
                break;
            }
        }
        /* if there is no name configured, us the number as name */
        if (name == NULL) {
            string = xasprintf("%" PRIu32, workspaces[i].id);
            LIST_APPEND(strings, string, strlen(string) + 1);
            free(string);
        } else {
            LIST_APPEND(strings, name, strlen(name) + 1);
        }
    }

    xcb_ewmh_coordinates_t zero_coordinates[workspaces_length];
    memset(zero_coordinates, 0, sizeof(zero_coordinates));
    xcb_ewmh_set_desktop_viewport(display.ewmh, display.screen_index, workspaces_length, zero_coordinates);

    xcb_ewmh_set_desktop_names(display.ewmh, display.screen_index, strings_length, strings);
    free(strings);

    xcb_ewmh_set_number_of_desktops(display.ewmh, display.screen_index, workspaces_length);

    return &workspaces[index];
}

/* Get the currently active workspace. */
static struct workspace *get_active_workspace(void)
{
    for (size_t i = 0; i < workspaces_length; i++) {
        if (workspaces[i].state == WORKSPACE_ACTIVE) {
            return &workspaces[i];
        }
    }
    return NULL;
}

/* Add a new window to a workspace. */
bool add_window_to_workspace(workspace_t id, xcb_window_t window)
{
    struct workspace *workspace;

    if (id == WORKSPACE_NONE) {
        workspace = get_active_workspace();
    } else {
        workspace = get_workspace(id);
    }
    LIST_APPEND_VALUE(workspace->windows, window);
    /* TODO: if tiling, reposition/resize existing windows */

    notef("window %#" PRIx32 " added to workspace %" PRIu32 "\n", window, workspace->id);

    return workspace->state >= WORKSPACE_VISIBLE;
}

/* Remove the window from its current workspace. */
void remove_window_from_workspace(xcb_window_t window)
{
    /* find any workspace this window is on and remove it */
    for (size_t i = 0; i < workspaces_length; i++) {
        for (size_t j = 0; j < workspaces[i].windows_length; j++) {
            if (workspaces[i].windows[j] == window) {
                workspaces[i].windows_length--;
                MOVE(&workspaces[i].windows[j], &workspaces[i].windows[j],
                        workspaces[i].windows_length - j);
                /* TODO: if tiling, reposition/resize existing windows */
                notef("window %#" PRIx32 " removed from workspace %" PRIu32 "\n", window, workspaces[i].id);
                return;
            }
        }
    }
}

/* Focus the workspace identified by @id. */
void focus_workspace(workspace_t id)
{
    struct workspace *old_workspace = NULL;
    struct workspace *workspace = NULL;

    old_workspace = get_active_workspace();
    if (old_workspace->id == id) {
        /* the focus did not change */
        return;
    }

    workspace = get_workspace(id);
    if (workspace == NULL) {
        workspace = add_workspace(id);
        workspace->crtc = old_workspace->crtc;
        /* TODO: get preferred output */
    }

    /* map new windows if the workspace was previously hidden */
    if (workspace->state == WORKSPACE_HIDDEN) {
        for (size_t i = 0; i < workspace->windows_length; i++) {
            xcb_map_window(display.xcb, workspace->windows[i]);
        }
    }

    /* unmap old windows if the old workspace was on the same monitor */
    if (old_workspace->crtc == workspace->crtc) {
        old_workspace->state = WORKSPACE_HIDDEN;
        for (size_t i = 0; i < old_workspace->windows_length; i++) {
            xcb_unmap_window(display.xcb, old_workspace->windows[i]);
        }
    } else {
        old_workspace->state = WORKSPACE_VISIBLE;
    }

    workspace->state = WORKSPACE_ACTIVE;

    notef("workspace %" PRIu32 " focused\n", id);

    const size_t index = workspace - workspaces;
    xcb_ewmh_set_current_desktop(display.ewmh, display.screen_index, index);
}

/* Notify the workspace module that a new monitor now exists. */
void report_monitor_change_to_workspaces(xcb_randr_crtc_t crtc, 
        const struct rectangle *old_rectangle, const struct rectangle *new_rectangle)
{
    workspace_t id = WORKSPACE_NONE;
    struct workspace *workspace = NULL;

    if (old_rectangle->width == 0) {
        if (new_rectangle->width == 0) {
            return;
        }

        /* TODO: use configuration */
        if (workspaces_length == 0) {
            id = WORKSPACE_FIRST;
        } else {
            /* TODO: use a smarter ID */
            id = workspaces[workspaces_length - 1].id + 1;
        }
        workspace = add_workspace(id);
        /* if this is the first workspace to exist, make it active */
        if (workspaces_length == 1) {
            workspace->state = WORKSPACE_ACTIVE;
            xcb_ewmh_set_current_desktop(display.ewmh, display.screen_index, 0);
        } else {
            workspace->state = WORKSPACE_VISIBLE;
        }
        workspace->crtc = crtc;
    } else {
        /* find all workspaces with that crtc */
        for (size_t i = 0; i < workspaces_length; i++) {
            if (workspaces[i].crtc == crtc) {
                id = workspaces[i].id;
                if (new_rectangle->width == 0) {
                    /* TODO: disable this workspace */
                } else if (old_rectangle->x != new_rectangle->x ||
                        old_rectangle->y != new_rectangle->y ||
                        old_rectangle->width != new_rectangle->width ||
                        old_rectangle->height != new_rectangle->height) {
                    /* TODO: redo tiling */
                }
            }
        }
    }
}

/* Get the workspace @window is part of. */
static struct workspace *get_window_workspace(xcb_window_t window)
{
    for (size_t i = 0; i < workspaces_length; i++) {
        for (size_t j = 0; j < workspaces[i].windows_length; j++) {
            if (workspaces[i].windows[j] == window) {
                return &workspaces[i];
            }
        }
    }
    return NULL;
}

/* Notify the workspace module that a window has moved. */
void report_window_movement_to_workspaces(xcb_window_t window, const struct rectangle *rectangle)
{
    xcb_randr_crtc_t monitor;
    struct workspace *window_workspace, *monitor_workspace = NULL;

    monitor = get_monitor_from_rectangle(rectangle);
    if (monitor == XCB_NONE) {
        return;
    }

    window_workspace = get_window_workspace(window);
    if (window_workspace == NULL) {
        return;
    }

    for (size_t i = 0; i < workspaces_length; i++) {
        if (workspaces[i].crtc == monitor) {
            monitor_workspace = &workspaces[i];
            break;
        }
    }
    ASSERT(monitor_workspace != NULL, "each monitor must have a workspace");

    if (window_workspace != monitor_workspace) {
        remove_window_from_workspace(window);
        add_window_to_workspace(monitor_workspace->id, window);
    }
}

/* Notify the workspace module that the focus has changed. */
void report_focus_change_to_workspaces(xcb_window_t window)
{
    xcb_randr_crtc_t monitor;
    struct rectangle rectangle;

    if (window == display.root) {
        /* since the root stretches all workspaces, no change required */
        return;
    }

    /* find any workspace this window is on and focus that workspace */
    for (size_t i = 0; i < workspaces_length; i++) {
        for (size_t j = 0; j < workspaces_length; j++) {
            if (workspaces[i].windows[j] == window) {
                focus_workspace(workspaces[i].id);
                return;
            }
        }
    }

    /* the window is not on any workspace, find the monitor it is on and from
     * there the workspace
     */
    get_window_rectangle(window, &rectangle);
    monitor = get_monitor_from_rectangle(&rectangle);
    for (size_t i = 0; i < workspaces_length; i++) {
        if (workspaces[i].crtc == monitor) {
            focus_workspace(workspaces[i].id);
            return;
        }
    }

    /* if this point is reached, the window must be outside */
}
