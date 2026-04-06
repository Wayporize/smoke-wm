#include <stdlib.h>

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

/* Get the path of the configuration to use on startup. */
char *get_configuration_path(void)
{
    /* TODO: */
    return NULL;
}
