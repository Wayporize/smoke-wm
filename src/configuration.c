#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

/**
 * This file handles the management of configuration objects and related parts
 * of the configuration like the configuration directory.
 */

#include "configuration.h"

/* the globally accessible configuration object */
struct wm Configuration;

/* the basic configuration default options */
struct wm Configuration_default = {
    .tiling.layout = TILE_AUTO,
    .tiling.gaps.inner[0] = 4,
    .tiling.gaps.inner[1] = 4,
    .tiling.gaps.inner[2] = 4,
    .tiling.gaps.inner[3] = 4,
    .border.size = 2,
    .border.decoration = BORDER_FULL,
    .border.radius.inner = 7,
    .border.radius.outer = 8,
    /* TODO: these are just placeholder colors */
    .border.color.focused.alpha = 0xffff,
    .border.color.focused.red = 0xffff,
    .border.color.highlight.alpha = 0xffff,
    .border.color.highlight.green = 0xffff,
    .border.color.inactive.alpha = 0xffff,
    .border.color.inactive.blue = 0xffff
};

/* Clear a configuration object. */
void clear_configuration(struct wm *wm)
{
    for (size_t i = 0; i < wm->monitor_length; i++) {
        free(wm->monitor[i].name);
    }
    free(wm->monitor);

    for (size_t i = 0; i < wm->workspace_length; i++) {
        free(wm->workspace[i].name);
        free(wm->workspace[i].monitor);
    }
    free(wm->workspace);

    for (size_t i = 0; i < wm->window_length; i++) {
        free(wm->window[i].name);
        free(wm->window[i].class);
        free(wm->window[i].instance);
        free(wm->window[i].workspace);
        free(wm->window[i].monitor);
    }
    free(wm->window);

    for (size_t i = 0; i < wm->binding_length; i++) {
        /* wm->binding[i].value TODO: clear action value */
    }
    free(wm->binding);

    for (size_t i = 0; i < wm->startup_length; i++) {
        /* wm->startup[i].value TODO: clear action value */
    }
    free(wm->startup);
}

/* get access to the user home directory */
extern char *user_home;

/* Check if given file exists and if it can be read. */
static bool is_readable(const char *path)
{
    if (access(path, R_OK) != 0) {
        if (errno != ENOENT) {
            /* the file exists but can not be read */
            printf("can not open %s: %s\n", path, strerror(errno));
        }
        return false;
    }
    return true;
}

/* Get the path of the configuration to use on startup. */
char *get_configuration_path(void)
{
    const char *xdg_config_home, *xdg_config_dirs;
    char *path = NULL;
    const char *colon;
    int length;

    /* search for the configuration file smoke-wm/config.toml within these
     * directories (ordered by preference):
     * XDG_CONFIG_HOME:XDG_CONFIG_DIRS:~/.config:/etc/xdg
     */

    xdg_config_home = getenv("XDG_CONFIG_HOME");
    if (xdg_config_home != NULL && xdg_config_home[0] != '\0') {
        path = xasprintf("%s/smoke-wm/config.toml", xdg_config_home);
        if (!is_readable(path)) {
            free(path);
            path = NULL;
        }
    }

    if (path == NULL) {
        xdg_config_dirs = getenv("XDG_CONFIG_DIRS");
        if (xdg_config_dirs != NULL && xdg_config_dirs[0] != '\0') {
            do {
                colon = strchr(xdg_config_dirs, ':');
                if (colon != NULL) {
                    length = colon - xdg_config_dirs;
                } else {
                    length = strlen(xdg_config_dirs);
                }

                path = xasprintf("%.*s/smoke-wm/config.toml",
                        length, xdg_config_dirs);
                if (is_readable(path)) {
                    break;
                }
                free(path);
                path = NULL;

                xdg_config_dirs = colon + 1;
            } while (colon != NULL);
        }
    }

    if (path == NULL) {
        path = xasprintf("%s/.config/smoke-wm/config.toml", user_home);
        if (!is_readable(path)) {
            free(path);
            path = xstrdup("/etc/xdg/smoke-wm/config.toml");
            if (!is_readable(path)) {
                free(path);
                path = NULL;
            }
        }
    }

    return path;
}
