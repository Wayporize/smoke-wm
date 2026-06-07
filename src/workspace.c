#include <inttypes.h>
#include <utility/log.h>
#include <utility/utility.h>

#include "configuration.h"
#include "display.h"
#include "workspace.h"

/* list of all workspaces sorted by their ID, each workspace in this list has an
 * associated monitor
 * TODO: hashmap
 */
STATIC_LIST(struct workspace, workspaces);

/* TODO: integrate tiling directly into here */

/* Get the data of a workspace. */
static struct workspace *get_workspace(const utf8_t *name)
{
    for (size_t i = 0; i < workspaces_length; i++) {
        if (strcmp(workspaces[i].name, name) == 0) {
            return &workspaces[i];
        }
    }
    return NULL;
}

/* Add a non-existent workspace to `workspaces`. */
static struct workspace *add_workspace(const utf8_t *name, xcb_randr_crtc_t monitor)
{
    struct workspace *workspace;

    /* append the new workspace to the end */
    LIST_APPEND(workspaces, NULL, 1);
    workspace = &workspaces[workspaces_length - 1];
    workspace->name = xstrdup(name);
    notef("workspace %s added\n", name);

    xcb_ewmh_coordinates_t zero_coordinates[workspaces_length];
    memset(zero_coordinates, 0, sizeof(zero_coordinates));
    xcb_ewmh_set_desktop_viewport(display.ewmh, display.screen_index, workspaces_length, zero_coordinates);

    LIST(utf8_t, strings);
    LIST_INITIALIZE(strings, 64);
    for (size_t i = 0; i < workspaces_length; i++) {
        LIST_APPEND(strings, workspaces[i].name, strlen(workspaces[i].name) + 1);
    }
    xcb_ewmh_set_desktop_names(display.ewmh, display.screen_index, strings_length, strings);
    free(strings);

    xcb_ewmh_set_number_of_desktops(display.ewmh, display.screen_index, workspaces_length);

    workspace->crtc = monitor;
    notef("workspace %s is now associated to monitor %" PRIu32 "\n", workspace->name, workspace->crtc);

    return workspace;
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

/* Get the user configured monitor for the workspace with name @name. */
static xcb_randr_crtc_t get_configured_monitor(const utf8_t *name)
{
    for (size_t i = 0; i < Configuration.workspace_length; i++) {
        utf8_t *const output = Configuration.workspace[i].output;
        if (output == NULL) {
            /* no output configured */
            continue;
        }
        if (strcmp(Configuration.workspace[i].name, name) == 0) {
            return get_monitor_from_output_name(output);
        }
    }
    return XCB_NONE;
}

/* Add a new window to a workspace. */
void add_window_to_workspace(struct window *window)
{
    struct wm_window configuration;
    struct workspace *workspace = NULL;

    /* get the configured workspace for this window */
    if (get_window_configuration(window, &configuration) && configuration.workspace != NULL) {
        workspace = get_workspace(configuration.workspace);
        if (workspace == NULL) {
            const xcb_randr_crtc_t monitor = get_configured_monitor(configuration.workspace);
            if (monitor != XCB_NONE) {
                workspace = add_workspace(configuration.workspace, monitor);
            }
        }
    }

    if (workspace == NULL) {
        workspace = get_active_workspace();
    }

    LIST_APPEND_VALUE(workspace->windows, window);
    /* TODO: if tiling, reposition/resize existing windows */
    if (window->state == XCB_ICCCM_WM_STATE_NORMAL && workspace->state == WORKSPACE_HIDDEN) {
        window->state = XCB_ICCCM_WM_STATE_ICONIC;
    }

    notef("window %#" PRIx32 " added to workspace %s\n", window->id, workspace->name);
}

/* Remove the window from its current workspace. */
void remove_window_from_workspace(struct window *window)
{
    /* find any workspace this window is on and remove it */
    for (size_t i = 0; i < workspaces_length; i++) {
        for (size_t j = 0; j < workspaces[i].windows_length; j++) {
            if (workspaces[i].windows[j] == window) {
                workspaces[i].windows_length--;
                MOVE(&workspaces[i].windows[j], &workspaces[i].windows[j],
                        workspaces[i].windows_length - j);
                /* TODO: if tiling, reposition/resize existing windows */
                notef("window %#" PRIx32 " removed from workspace %s\n", window->id, workspaces[i].name);
                return;
            }
        }
    }
}

/* Focus the workspace identified by @id. */
void focus_workspace(const utf8_t *name)
{
    struct workspace *old_workspace = NULL;
    struct workspace *workspace = NULL;

    old_workspace = get_active_workspace();
    if (strcmp(old_workspace->name, name) == 0) {
        /* the focus did not change */
        return;
    }

    workspace = get_workspace(name);
    if (workspace == NULL) {
        xcb_randr_crtc_t monitor;

        monitor = get_configured_monitor(name);
        if (monitor == XCB_NONE) {
            monitor = old_workspace->crtc;
        }
        workspace = add_workspace(name, monitor);
    }

    /* map new windows if the workspace was previously hidden */
    if (workspace->state == WORKSPACE_HIDDEN) {
        for (size_t i = 0; i < workspace->windows_length; i++) {
            xcb_map_window(display.xcb, workspace->windows[i]->id);
        }
    }

    /* unmap old windows if the old workspace was on the same monitor */
    if (old_workspace->crtc == workspace->crtc) {
        old_workspace->state = WORKSPACE_HIDDEN;
        for (size_t i = 0; i < old_workspace->windows_length; i++) {
            xcb_unmap_window(display.xcb, old_workspace->windows[i]->id);
        }
    } else {
        old_workspace->state = WORKSPACE_VISIBLE;
    }

    workspace->state = WORKSPACE_ACTIVE;

    notef("workspace %s focused\n", name);

    const size_t index = workspace - workspaces;
    xcb_ewmh_set_current_desktop(display.ewmh, display.screen_index, index);
}

/* Compare the values two `uint32_t` pointers point to. */
static int compare_uint32(const void *a, const void *b)
{
    return *((uint32_t*) a) - *((uint32_t*) b);
}

/* Get a sorted list of used monitors terminated by `XCB_NONE`. */
static xcb_randr_crtc_t *get_used_monitors(void)
{
    LIST(xcb_randr_crtc_t, monitors);

    LIST_INITIALIZE(monitors, 8);
    for (size_t i = 0, j; i < workspaces_length; i++) {
        for (j = 0; j < monitors_length; j++) {
            if (workspaces[i].crtc == monitors[j]) {
                break;
            }
        }
        if (j == monitors_length) {
            LIST_APPEND_VALUE(monitors, workspaces[i].crtc);
        }
    }

    SORT(monitors, monitors_length, compare_uint32);
    LIST_APPEND_VALUE(monitors, XCB_NONE);
    LIST_TRIM(monitors);
    return monitors;
}

/* Create a fallback workspace in case a monitor has no workspaces. */
static void create_fallback_workspace(xcb_randr_crtc_t monitor)
{
    utf8_t name[64];
    size_t end = sizeof(name) - 2;
    size_t index = end;
    struct workspace *workspace;

    memset(name, '0', end);
    name[end] = '1';
    name[end + 1] = '\0';
    while (get_workspace(&name[index]) != NULL) {
        size_t i;

        for (i = end; name[i] == '9'; i--) {
            name[i] = '0';
        }
        name[i]++;
        index = MIN(index, i);
    }
    workspace = add_workspace(&name[index], monitor);
    /* if this is the first workspace to exist, make it active */
    if (workspaces_length == 1) {
        workspace->state = WORKSPACE_ACTIVE;
        xcb_ewmh_set_current_desktop(display.ewmh, display.screen_index, 0);
    } else {
        workspace->state = WORKSPACE_VISIBLE;
    }
}

/* Handle the appearance of a new monitor. */
static void register_new_monitor(xcb_randr_crtc_t crtc)
{
    /* TODO: use configuration */
    create_fallback_workspace(crtc);
}

/* Notify the workspace module that a new monitor now exists. */
void report_monitor_change_to_workspaces(xcb_randr_crtc_t crtc, 
        const struct rectangle *old_rectangle, const struct rectangle *new_rectangle)
{
    if (old_rectangle->width == 0) {
        if (new_rectangle->width == 0) {
            return;
        }
        register_new_monitor(crtc);
    } else {
        /* find all workspaces with that crtc */
        for (size_t i = 0; i < workspaces_length; i++) {
            if (workspaces[i].crtc == crtc) {
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
static struct workspace *get_window_workspace(struct window *window)
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

/* Notify the workspace module that the window got a map request. */
void relay_map_request_to_workspaces(struct window *window)
{
    struct workspace *const workspace = get_window_workspace(window);
    if (workspace != NULL &&
            workspace->state == WORKSPACE_HIDDEN &&
            window->state == XCB_ICCCM_WM_STATE_NORMAL) {
        window->state = XCB_ICCCM_WM_STATE_ICONIC;
    }
}

/* Notify the workspace module that a window has moved. */
void report_window_movement_to_workspaces(struct window *window)
{
    xcb_randr_crtc_t monitor;
    struct workspace *window_workspace, *monitor_workspace = NULL;

    monitor = get_monitor_from_rectangle(window->x, window->y, window->width, window->height);
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
        LIST_APPEND_VALUE(monitor_workspace->windows, window);
        notef("window %#" PRIx32 " switched to workspace %s\n", window->id, monitor_workspace->name);
    }
}

/* Notify the workspace module that the focus has changed. */
void report_focus_change_to_workspaces(struct window *window)
{
    xcb_randr_crtc_t monitor;

    /* find any workspace this window is on and focus that workspace */
    for (size_t i = 0; i < workspaces_length; i++) {
        for (size_t j = 0; j < workspaces[i].windows_length; j++) {
            if (workspaces[i].windows[j] == window) {
                focus_workspace(workspaces[i].name);
                return;
            }
        }
    }

    /* the window is not on any workspace, find the monitor it is on and from
     * there the workspace
     */
    monitor = get_monitor_from_rectangle(window->x, window->y, window->width, window->height);
    for (size_t i = 0; i < workspaces_length; i++) {
        if (workspaces[i].crtc == monitor) {
            focus_workspace(workspaces[i].name);
            return;
        }
    }

    /* if this point is reached, the window must be outside */
}

/* Notify the workspace module that the configuration has changed. */
void report_configuration_change_to_workspaces(struct wm *configuration)
{
    xcb_randr_crtc_t *const previous_usage = get_used_monitors();

    /* move the workspaces to their configured output */
    for (size_t i = 0; i < configuration->workspace_length; i++) {
        struct workspace *workspace;

        utf8_t *const output = configuration->workspace[i].output;
        if (output == NULL) {
            /* no output configured */
            continue;
        }
        const xcb_randr_crtc_t monitor = get_monitor_from_output_name(output);
        if (monitor == XCB_NONE) {
            /* the output does not exist or no crtc is associated to it */
            continue;
        }
        workspace = get_workspace(configuration->workspace[i].name);
        if (workspace == NULL) {
            /* find a workspace on the monitor that is empty */
            for (size_t i = 0; i < workspaces_length; i++) {
                if (workspaces[i].crtc == monitor && workspaces[i].windows_length == 0) {
                    workspace = &workspaces[i];
                    break;
                }
            }
            if (workspace == NULL) {
                continue;
            }

            notef("workspace %s changes name to %s\n", workspace->name, configuration->workspace[i].name);
            free(workspace->name);
            workspace->name = xstrdup(configuration->workspace[i].name);
        }
        /* TODO: move windows */
        workspace->crtc = monitor;
        notef("workspace %s is now associated to monitor %" PRIu32 "\n", workspace->name, workspace->crtc);
    }

    /* Now some monitors might have no workspace, we must create a default
     * workspace.
     */
    xcb_randr_crtc_t *const usage = get_used_monitors();
    for (size_t i = 0, j = 0; previous_usage[i] != XCB_NONE; i++) {
        if (usage[j] != previous_usage[i]) {
            create_fallback_workspace(previous_usage[i]);
        } else {
            j++;
        }
    }
    free(previous_usage);
    free(usage);

    for (size_t i = 0; i < configuration->window_length; i++) {
        /* TODO: */
    }
}
