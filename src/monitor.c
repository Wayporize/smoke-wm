#include <inttypes.h>
#include <utility/list.h>
#include <utility/log.h>
#include <utility/utility.h>

#include "configuration.h"
#include "display.h"
#include "monitor.h"
#include "window.h"

/* list of output devices the user has */
STATIC_LIST(struct output, outputs);

/* sub regions of the screen */
STATIC_LIST(struct monitor*, monitors);

/* list of all workspaces TODO: sort by name? */
STATIC_LIST(struct workspace*, workspaces);

/* primary output device */
xcb_randr_output_t primary;

/* Get information about an output. */
static struct output *get_output(xcb_randr_output_t id)
{
    for (size_t i = 0; i < outputs_length; i++) {
        if (outputs[i].id == id) {
            return &outputs[i];
        }
    }
    return NULL;
}

/* Get information about a monitor/crtc. */
static struct monitor *get_monitor(xcb_randr_crtc_t id)
{
    for (size_t i = 0; i < monitors_length; i++) {
        if (monitors[i]->id == id) {
            return monitors[i];
        }
    }
    return NULL;
}

/* Get the data of a workspace. */
static struct workspace *get_workspace(const utf8_t *name)
{
    for (size_t i = 0; i < workspaces_length; i++) {
        if (strcmp(workspaces[i]->name, name) == 0) {
            return workspaces[i];
        }
    }
    return NULL;
}

/* Get the currently active monitor. */
struct monitor *get_active_monitor(void)
{
    for (size_t i = 0; i < monitors_length; i++) {
        for (size_t j = 0; j < monitors[i]->workspaces_length; j++) {
            if (monitors[i]->workspaces[j]->state == WORKSPACE_ACTIVE) {
                return monitors[i];
            }
        }
    }
    return NULL;
}

/* Get the monitor a workspace is on. */
struct monitor *get_workspace_monitor(struct workspace *workspace)
{
    for (size_t i = 0; i < monitors_length; i++) {
        for (size_t j = 0; j < monitors[i]->workspaces_length; j++) {
            if (monitors[i]->workspaces[j] == workspace) {
                return monitors[i];
            }
        }
    }
    return NULL;
}

/* Get the workspace a window is on. */
struct workspace *get_window_workspace(struct window *window)
{
    for (size_t i = 0; i < workspaces_length; i++) {
        for (size_t j = 0; j < workspaces[i]->windows_length; j++) {
            if (workspaces[i]->windows[j] == window) {
                return workspaces[i];
            }
        }
    }
    return NULL;
}

/* Change the monitor a workspace is on and update all windows on the workspace.
 */
static void update_workspace(struct workspace *workspace)
{
    struct monitor *const monitor = get_workspace_monitor(workspace);
    /* stack all windows on top of each other */
    for (size_t i = 0; i < workspace->windows_length; i++) {
        struct window *const window = workspace->windows[i];
        const uint16_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
        int32_t min_width, max_width, min_height, max_height;
        if ((window->normal_hints.flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE)) {
            min_width = window->normal_hints.min_width;
            min_height = window->normal_hints.min_height;
        } else {
            min_width = 0;
            min_height = 0;
        }
        if ((window->normal_hints.flags & XCB_ICCCM_SIZE_HINT_P_MAX_SIZE)) {
            max_width = window->normal_hints.max_width;
            max_height = window->normal_hints.max_height;
        } else {
            max_width = INT32_MAX;
            max_height = INT32_MAX;
        }
        const uint32_t values[] = {
            monitor->x, monitor->y,
            MAX(MIN(monitor->width, max_width), min_width),
            MAX(MIN(monitor->height, max_height), min_height)
        };
        xcb_configure_window(display.xcb, window->id, mask, values);
    }
}

/* Update all relevant EWMH properties. */
static void update_ewmh_desktop_properties(void)
{
    xcb_ewmh_coordinates_t zero_coordinates[workspaces_length];
    memset(zero_coordinates, 0, sizeof(zero_coordinates));
    xcb_ewmh_set_desktop_viewport(display.ewmh, display.screen_index, workspaces_length, zero_coordinates);

    LIST(utf8_t, strings);
    LIST_INITIALIZE(strings, 64);
    for (size_t i = 0; i < workspaces_length; i++) {
        LIST_APPEND(strings, workspaces[i]->name, strlen(workspaces[i]->name) + 1);
    }
    xcb_ewmh_set_desktop_names(display.ewmh, display.screen_index, strings_length, strings);
    free(strings);

    xcb_ewmh_set_number_of_desktops(display.ewmh, display.screen_index, workspaces_length);

    for (size_t i = 0; i < workspaces_length; i++) {
        if (workspaces[i]->state == WORKSPACE_ACTIVE) {
            xcb_ewmh_set_current_desktop(display.ewmh, display.screen_index, i);
            break;
        }
    }
}

/* Add a non-existent workspace to `workspaces`. */
static struct workspace *create_workspace(struct monitor *monitor, const utf8_t *name)
{
    struct workspace *workspace;
    utf8_t fallback_name[64];

    /* use the next name if `NULL` is specified */
    if (name == NULL) {
        size_t end = sizeof(fallback_name) - 2;
        size_t index = end;

        memset(fallback_name, '0', end);
        fallback_name[end] = '1';
        fallback_name[end + 1] = '\0';
        while (get_workspace(&fallback_name[index]) != NULL) {
            size_t i;

            for (i = end; fallback_name[i] == '9'; i--) {
                fallback_name[i] = '0';
            }
            fallback_name[i]++;
            index = MIN(index, i);
        }
        name = &fallback_name[index];
    }

    /* append the new workspace to the end */
    ALLOCATE_ZERO(workspace, 1);
    LIST_APPEND_VALUE(workspaces, workspace);
    workspace->name = xstrdup(name);
    if (monitor != NULL) {
        LIST_APPEND_VALUE(monitor->workspaces, workspace);
    }
    notef("workspace %s added to monitor %" PRIu32 "\n", name, monitor->id);

    /* if this is the first workspace to exist, make it active */
    if (workspaces_length == 1) {
        workspace->state = WORKSPACE_ACTIVE;
    } else {
        workspace->state = WORKSPACE_VISIBLE;
    }

    update_ewmh_desktop_properties();

    return workspace;
}

/* Get the currently active workspace. */
static struct workspace *get_active_workspace(void)
{
    for (size_t i = 0; i < workspaces_length; i++) {
        if (workspaces[i]->state == WORKSPACE_ACTIVE) {
            return workspaces[i];
        }
    }
    /* there is no workspace, need to add a placeholder workspace */
    return create_workspace(NULL, NULL);
}

/* Dump the monitor setup to stdout. */
static void dump_monitor_setup(void)
{
    printf("primary %" PRIu32 "\n", primary);
    for (size_t i = 0; i < monitors_length; i++) {
        printf("monitor %u: %" PRId32 "x%" PRId32 "+%" PRId32 "+%" PRId32 " %u\n",
                monitors[i]->id, monitors[i]->width, monitors[i]->height,
                monitors[i]->x, monitors[i]->y, monitors[i]->rotation);
    }
    for (size_t i = 0; i < outputs_length; i++) {
        printf("output %u: %s %" PRIu32 " %s\n", outputs[i].id,
                outputs[i].name, outputs[i].crtc,
                outputs[i].connection == XCB_RANDR_CONNECTION_CONNECTED ? "connected" :
                outputs[i].connection == XCB_RANDR_CONNECTION_DISCONNECTED ? "disconnected" :
                "unknown");
    }
}

/* Initialize the output and monitor list with the current RandR configuration. */
void initialize_monitor_setup(xcb_randr_get_screen_resources_cookie_t cookie)
{
    xcb_randr_get_screen_resources_reply_t *reply;
    xcb_randr_output_t *output_ids;
    xcb_randr_crtc_t *monitor_ids;
    xcb_randr_get_output_primary_cookie_t primary_cookie;
    xcb_randr_get_output_primary_reply_t *primary_reply;

    reply = xcb_randr_get_screen_resources_reply(display.xcb, cookie, NULL);
    ASSERT(reply != NULL, "could not get screen resources");

    output_ids = xcb_randr_get_screen_resources_outputs(reply);
    outputs_length = xcb_randr_get_screen_resources_outputs_length(reply);
    monitor_ids = xcb_randr_get_screen_resources_crtcs(reply);
    monitors_length = xcb_randr_get_screen_resources_crtcs_length(reply);

    /* send out a bunch of requests at once */
    xcb_randr_get_output_info_cookie_t output_info_cookies[outputs_length];
    for (size_t i = 0; i < outputs_length; i++) {
        output_info_cookies[i] = xcb_randr_get_output_info(display.xcb,
                output_ids[i], reply->config_timestamp);
    }

    xcb_randr_get_crtc_info_cookie_t crtc_info_cookies[monitors_length];
    for (size_t i = 0; i < monitors_length; i++) {
        crtc_info_cookies[i] = xcb_randr_get_crtc_info(display.xcb,
                monitor_ids[i], reply->config_timestamp);
    }

    primary_cookie = xcb_randr_get_output_primary(display.xcb, display.root);

    /* start filling our local output/monitor configuration */
    ALLOCATE_ZERO(outputs, outputs_length);
    ALLOCATE(monitors, monitors_length);

    /* get information about all outputs */
    for (size_t i = 0; i < outputs_length; i++) {
        xcb_randr_get_output_info_reply_t *info_reply;
        uint8_t *name;
        int name_length;

        info_reply = xcb_randr_get_output_info_reply(display.xcb, output_info_cookies[i], NULL);
        ASSERT(info_reply != NULL, "could not get output info");

        outputs[i].id = output_ids[i];
        name = xcb_randr_get_output_info_name(info_reply);
        name_length = xcb_randr_get_output_info_name_length(info_reply);
        outputs[i].name = xstrndup((char*) name, name_length);
        outputs[i].crtc = info_reply->crtc;
        outputs[i].connection = info_reply->connection;

        free(info_reply);
    }

    /* get information about all monitors */
    for (size_t i = 0; i < monitors_length; i++) {
        xcb_randr_get_crtc_info_reply_t *info_reply;
        struct monitor *monitor;

        info_reply = xcb_randr_get_crtc_info_reply(display.xcb, crtc_info_cookies[i], NULL);
        ASSERT(info_reply != NULL, "could not get crtc info");

        ALLOCATE_ZERO(monitor, 1);
        monitor->id = monitor_ids[i];
        monitor->mode = info_reply->mode;
        monitor->x = info_reply->x;
        monitor->y = info_reply->y;
        monitor->width = info_reply->width;
        monitor->height = info_reply->height;
        monitor->rotation = info_reply->rotation;
        /* create an initial workspace for this monitor */
        (void) create_workspace(monitor, NULL);
        monitors[i] = monitor;

        free(info_reply);
    }

    free(reply);

    primary_reply = xcb_randr_get_output_primary_reply(display.xcb, primary_cookie, NULL);
    ASSERT(primary_reply != NULL, "failed to get primary output");
    primary = primary_reply->output;
    free(primary_reply);

    /* associated to test "randr-setup" */
    notef("start of dumping monitor setup\n");
    dump_monitor_setup();
    notef("end of dumping monitor setup\n");
}

/* Get the monitor that is projected onto the output with given name. */
struct monitor *get_monitor_from_output_name(const utf8_t *name)
{
    for (size_t i = 0; i < outputs_length; i++) {
        if (strcmp(outputs[i].name, name) == 0) {
            return get_monitor(outputs[i].crtc);
        }
    }
    return NULL;
}

/* Get the overlapping area between two rectangles. */
static inline int64_t get_overlapping_area(
        int32_t a_x, int32_t a_y, int32_t a_width, int32_t a_height,
        int32_t b_x, int32_t b_y, int32_t b_width, int32_t b_height)
{
    int32_t x, y;

    x = MIN(a_x + a_width, b_x + b_width);
    x -= MAX(a_x, b_x);

    y = MIN(a_y + a_height, b_y + b_height);
    y -= MAX(a_y, b_y);

    if (x > 0 && y > 0) {
        return (int64_t) x * y;
    } else {
        return 0;
    }
}

/* Get the monitor that intersects given rectangle most. */
struct monitor *get_monitor_from_rectangle(int32_t x, int32_t y, int32_t width, int32_t height)
{
    struct monitor *best_monitor = NULL;
    int64_t best_area = 0;

    const int32_t center_x = x + width / 2;
    const int32_t center_y = y + height / 2;
    for (size_t i = 0; i < monitors_length; i++) {
        struct monitor *const monitor = monitors[i];
        if (monitor->mode == XCB_NONE) {
            continue;
        }

        /* check if the midpoint is inside the monitor */
        const int32_t relative_x = center_x - monitor->x;
        const int32_t relative_y = center_y - monitor->y;
        if (relative_x >= 0 && relative_y >= 0 &&
                relative_x < monitor->width &&
                relative_y < monitor->height) {
            return monitor;
        }

        /* check if the overlapping area is bigger than before */
        const int64_t area = get_overlapping_area(x, y, width, height,
                monitor->x, monitor->y, monitor->width, monitor->height);
        if (area > best_area) {
            best_area = area;
            best_monitor = monitor;
        }
    }

    return best_monitor;
}

/* Cache output properties. */
void change_output(xcb_randr_output_t output, xcb_randr_crtc_t crtc, xcb_randr_connection_t connection,
        xcb_timestamp_t config_timestamp)
{
    struct output *info;

    notef("randr: output %" PRIu32 " changed: %" PRIu32 " %u" "\n", output,
            crtc, connection);
    info = get_output(output);
    if (info == NULL) {
        xcb_randr_get_output_info_cookie_t cookie;
        xcb_randr_get_output_info_reply_t *reply;
        uint8_t *name;
        int name_length;

        notef("this output is new\n");

        /* send out a request for the name, this is so rare that it does not
         * need to be efficient, in fact it might never happen once in a user's
         * lifetime
         */
        cookie = xcb_randr_get_output_info(display.xcb, output, config_timestamp);
        reply = xcb_randr_get_output_info_reply(display.xcb, cookie, NULL);
        ASSERT(reply != NULL, "could not get output info");

        LIST_APPEND(outputs, NULL, 1);
        info = &outputs[outputs_length - 1];
        info->id = output;
        name = xcb_randr_get_output_info_name(reply);
        name_length = xcb_randr_get_output_info_name_length(reply);
        info->name = xstrndup((char*) name, name_length);
        info->crtc = reply->crtc;
        info->connection = reply->connection;
        free(reply);
        /* TODO: now maybe a workspace need to be added or windows configured
         * to be on this output should be moved to it if not explicitly moved
         * away some time in the past in case a crtc is present
         */
    } else {
        /* TODO: if the crtc changed, we need to inform the user as windows
         * might be hidden now
         */
        info->crtc = crtc;
        /* TODO: if the connection is disconnected, should it be treated the same
         * as no crtc?
         */
        info->connection = connection;
    }
}

/* Cache crtc properties. */
void change_crtc(xcb_randr_crtc_t crtc, xcb_randr_mode_t mode, xcb_randr_rotation_t rotation,
        int32_t x, int32_t y, int32_t width, int32_t height)
{
    struct monitor *monitor;

    notef("randr: crtc %" PRIu32 " changed: %" PRIu32 " %u %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 "\n",
            crtc, mode, rotation, x, y, width, height);
    monitor = get_monitor(crtc);
    if (monitor == NULL) {
        notef("this crtc is new\n");

        ALLOCATE_ZERO(monitor, 1);
        LIST_APPEND_VALUE(monitors, monitor);
        monitor->id = crtc;
    }

    if (mode != XCB_NONE) {
        monitor->x = x;
        monitor->y = y;
        monitor->width = width;
        monitor->height = height;
    }

    monitor->rotation = rotation;

    /* check if the monitor reappears */
    if (monitor->mode == XCB_NONE && mode != XCB_NONE) {
        for (size_t i = 0; i < workspaces_length; i++) {
            if (get_workspace_monitor(workspaces[i]) == NULL) {
                LIST_APPEND_VALUE(monitor->workspaces, workspaces[i]);
            }
        }
        /* TODO: use configuration to move workspaces/windows to this monitor */
        if (monitor->workspaces_length == 0) {
            (void) create_workspace(monitor, NULL);
        } else if (monitor->workspaces[monitor->workspaces_length - 1]->state == WORKSPACE_HIDDEN) {
            monitor->workspaces[monitor->workspaces_length - 1]->state = WORKSPACE_VISIBLE;
        }
    /* check if the monitor disappears */
    } else if (monitor->mode != XCB_NONE && mode == XCB_NONE) {
        LIST(struct window*, windows);
        bool is_active_workspace_gone = false;

        LIST_INITIALIZE(windows, 8);
        for (size_t i = 0; i < monitor->workspaces_length; i++) {
            struct workspace *const workspace = monitor->workspaces[i];

            LIST_APPEND(windows, workspace->windows, workspace->windows_length);

            notef("workspace %s removed\n", workspace->name);

            if (workspace->state == WORKSPACE_ACTIVE) {
                is_active_workspace_gone = true;
            }
            free(workspace->name);
            free(workspace->windows);
        }
        LIST_CLEAR(monitor->workspaces);

        /* if the active workspace got removed, another workspace needs to be
         * activated
         */
        if (is_active_workspace_gone) {
            /* TODO: to what workspace should the activity fall back to? */
            for (size_t i = 0; i < workspaces_length; i++) {
                if (workspaces[i]->state == WORKSPACE_VISIBLE) {
                    workspaces[i]->state = WORKSPACE_ACTIVE;
                    break;
                }
            }
        }

        /* re-assign all windows */
        if (windows_length > 0) {
            /* TODO: use configuration */
            struct workspace *const workspace = get_active_workspace();
            LIST_APPEND(workspace->windows, windows, windows_length);
            update_workspace(workspace);
        }
        free(windows);
    } else {
        for (size_t i = 0; i < monitor->workspaces_length; i++) {
            update_workspace(monitor->workspaces[i]);
        }
    }

    monitor->mode = mode;
}

/* Get the user configured monitor for the workspace with name @name. */
static struct monitor *get_configured_monitor(const utf8_t *name)
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
    return NULL;
}

/* Change the active workspace to @active. */
static void change_active_workspace(struct workspace *active)
{
    struct workspace *old_active;

    old_active = get_active_workspace();
    if (old_active == active) {
        /* the focus did not change */
        return;
    }

    /* map new windows if the workspace was previously hidden */
    if (active->state == WORKSPACE_HIDDEN) {
        for (size_t i = 0; i < active->windows_length; i++) {
            notef("showing window %#" PRIx32 "\n", active->windows[i]->id);
            xcb_map_window(display.xcb, active->windows[i]->id);
        }
    }

    /* unmap old windows if the old workspace was on the same monitor */
    if (get_workspace_monitor(old_active) == get_workspace_monitor(active)) {
        old_active->state = WORKSPACE_HIDDEN;
        for (size_t i = 0; i < old_active->windows_length; i++) {
            notef("hiding window %#" PRIx32 "\n", old_active->windows[i]->id);
            xcb_unmap_window(display.xcb, old_active->windows[i]->id);
        }
    } else {
        old_active->state = WORKSPACE_VISIBLE;
    }

    active->state = WORKSPACE_ACTIVE;

    notef("workspace %s focused\n", active->name);

    /* update the `_NET_CURRENT_DESKTOP` property */
    update_ewmh_desktop_properties();
}

/* Change a window to a different workspace. */
void change_window_workspace_directly(struct window *window, struct workspace *workspace)
{
    struct workspace *const current = get_window_workspace(window);
    if (current == workspace) {
        /* the window is already on this workspace */
        return;
    }

    /* make sure the window is not on any workspace yet */
    if (current != NULL) {
        remove_window_from_workspace(window);
    }

    LIST_APPEND_VALUE(workspace->windows, window);
    update_workspace(workspace);

    notef("window %#" PRIx32 " added to workspace %s\n", window->id, workspace->name);

    if (window->id == display.focus) {
        /* the focus follows the window */
        change_active_workspace(workspace);
    }
}

/* Add a window to a workspace. */
void add_window_to_workspace(const utf8_t *name, struct window *window)
{
    struct workspace *workspace = NULL;

    /* check if the window wants to be added to a specific workspace */
    if (name != NULL) {
        /* get the workspace and make sure it exists */
        workspace = get_workspace(name);
        if (workspace == NULL) {
            struct monitor *monitor = get_configured_monitor(name);
            if (monitor == NULL) {
                monitor = get_active_monitor();
            }
            workspace = create_workspace(monitor, name);
        }
    } else {
        workspace = get_active_workspace();
    }
    change_window_workspace_directly(window, workspace);
}

/* Remove the window from its current workspace. */
void remove_window_from_workspace(struct window *window)
{
    /* find any workspace this window is on and remove it */
    for (size_t i = 0; i < workspaces_length; i++) {
        for (size_t j = 0; j < workspaces[i]->windows_length; j++) {
            if (workspaces[i]->windows[j] == window) {
                LIST_REMOVE(workspaces[i]->windows, j, 1);
                update_workspace(workspaces[i]);
                notef("window %#" PRIx32 " removed from workspace %s\n", window->id, workspaces[i]->name);
                return;
            }
        }
    }
}

/* Change a window to the specified workspace. */
int change_window_workspace(struct window *window, const utf8_t *name)
{
    struct workspace *const workspace = get_workspace(name);
    if (workspace == NULL) {
        return 1;
    }
    change_window_workspace_directly(window, workspace);
    return 0;
}

/* Focus the workspace identified by @name. */
void focus_workspace(const utf8_t *name)
{
    struct workspace *workspace = NULL;

    workspace = get_workspace(name);
    if (workspace == NULL) {
        struct monitor *monitor;

        monitor = get_configured_monitor(name);
        if (monitor == NULL) {
            struct workspace *const active = get_active_workspace();
            monitor = get_workspace_monitor(active);
        }
        workspace = create_workspace(monitor, name);
    }

    change_active_workspace(workspace);

    /* focus the window on that workspace */
    struct window *candidate = NULL;
    uint64_t focus = 0;
    for (size_t i = 0; i < workspace->windows_length; i++) {
        if (!is_focusable(workspace->windows[i])) {
            continue;
        }
        if (focus < workspace->windows[i]->focus) {
            focus = workspace->windows[i]->focus;
            candidate = workspace->windows[i];
        }
    }
    focus_window(candidate);
}

/* Change the monitor the workspace is on. */
static void change_workspace_monitor(struct workspace *workspace, struct monitor *monitor)
{
    struct monitor *workspace_monitor;

    workspace_monitor = get_workspace_monitor(workspace);
    if (workspace_monitor == monitor) {
        /* nothing changed */
        return;
    }

    /* move all windows by the amount the workspace moved by
     * TODO: move all in bounds in case the monitor size is different
     */
    for (size_t j = 0; j < workspace->windows_length; j++) {
        workspace->windows[j]->x += monitor->x - workspace_monitor->x;
        workspace->windows[j]->y += monitor->y - workspace_monitor->y;
        const uint16_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y;
        const uint32_t values[] = { workspace->windows[j]->x, workspace->windows[j]->y };
        xcb_configure_window(display.xcb, workspace->windows[j]->id, mask, values);
    }

    /* detach the workspace from its monitor */
    for (size_t j = 0; j < workspace_monitor->workspaces_length; j++) {
        if (workspace_monitor->workspaces[j] == workspace) {
            LIST_REMOVE(workspace_monitor->workspaces, j, 1);
            break;
        }
    }

    /* the workspace will be shown */
    if (workspace->state == WORKSPACE_HIDDEN) {
        for (size_t i = 0; i < workspace->windows_length; i++) {
            xcb_map_window(display.xcb, workspace->windows[i]->id);
        }
    }

    /* hide the active workspace */
    for (size_t i = 0; i < monitor->workspaces_length; i++) {
        if (monitor->workspaces[i]->state >= WORKSPACE_VISIBLE) {
            /* the workspace takes over the state */
            workspace->state = monitor->workspaces[i]->state;

            monitor->workspaces[i]->state = WORKSPACE_HIDDEN;
            for (size_t i = 0; i < monitor->workspaces[i]->windows_length; i++) {
                xcb_unmap_window(display.xcb, monitor->workspaces[i]->windows[i]->id);
            }
            break;
        }
    }

    ASSERT(workspace->state != WORKSPACE_HIDDEN, "the workspace must be shown now");

    /* attach the workspace to the new monitor */
    LIST_APPEND_VALUE(monitor->workspaces, workspace);

    LOG("workspace %s is now associated to monitor %" PRIu32 "\n", workspace->name, monitor->id);
}

/* Move the current workspace to given destination.
 *
 * @destination can either be another workspace or an output.
 */
void move_workspace(const utf8_t *destination)
{
    struct workspace *const active = get_active_workspace();
    struct workspace *workspace = get_workspace(destination);
    struct monitor *monitor;
    if (workspace == NULL) {
        monitor = get_monitor_from_output_name(destination);
        if (monitor == NULL) {
            return;
        }
    } else {
        monitor = get_workspace_monitor(workspace);
    }
    change_workspace_monitor(active, monitor);
}

/* Rename the current workspace to @name. */
void rename_workspace(const utf8_t *name)
{
    struct workspace *const active = get_active_workspace();
    struct workspace *const workspace = get_workspace(name);
    notef("workspace %s renamed to");
    if (workspace != NULL) {
        free(workspace->name);
        workspace->name = active->name;
    } else {
        free(active->name);
    }
    active->name = xstrdup(name);
    printf(" %s\n", name);
}

/* Notify the workspace module that the focus has changed. */
void report_focus_change_to_workspaces(struct window *window)
{
    struct workspace *workspace;

    workspace = get_window_workspace(window);
    if (workspace != NULL) {
        change_active_workspace(workspace);
    } else {
        /* the window is not on any workspace, find the monitor it is on and
         * from there the workspace
         */
        struct monitor *const monitor = get_monitor_from_rectangle(window->x, window->y, window->width, window->height);
        for (size_t i = 0; i < monitor->workspaces_length; i++) {
            if (monitor->workspaces[i]->state == WORKSPACE_VISIBLE) {
                change_active_workspace(monitor->workspaces[i]);
                break;
            }
        }
    }
}

/* Notify the workspace module that the configuration has changed. */
void report_configuration_change_to_workspaces(struct wm_workspace *configured, size_t configured_length)
{
    struct workspace *workspace;

    /* interpret all configuration entries */
    for (size_t i = 0; i < configured_length; i++) {
        if (configured[i].name == NULL || configured[i].output == NULL) {
            /* ignore empty entries */
            continue;
        }

        struct monitor *const monitor = get_monitor_from_output_name(configured[i].output);
        if (monitor == NULL) {
            continue;
        }

        workspace = get_workspace(configured[i].name);
        if (workspace == NULL) {
            if (monitor->workspaces_length == 1 && monitor->workspaces[0]->windows_length == 0) {
                LOG("workspace %s changes name to %s\n", monitor->workspaces[0]->name, configured[i].name);
                free(monitor->workspaces[0]->name);
                monitor->workspaces[0]->name = xstrdup(configured[i].name);
            }
            /* no workspace that needs moving */
            continue;
        }

        if (monitor == get_workspace_monitor(workspace)) {
            LOG("ignoring workspace configuration (%s, %s) entry because nothing needs to change\n",
                    configured[i].name, configured[i].output);
            /* the workspace is already on the correct monitor */
            continue;
        }

        change_workspace_monitor(workspace, monitor);
    }

    /* create fallback workspaces for empty monitors */
    for (size_t i = 0; i < monitors_length; i++) {
        if (monitors[i]->workspaces_length == 0) {
            create_workspace(monitors[i], NULL);
        }
    }

    update_ewmh_desktop_properties();
}
