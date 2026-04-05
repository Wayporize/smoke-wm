#include <stdlib.h>

/**
 * This file handles the management of configuration objects and related parts
 * of the configuration like the configuration directory.
 */

#include "configuration.h"

/* the globally accessible configuration object */
struct wm Configuration;

/* Clear a configuration object. */
void clear_configuration(struct wm *wm)
{
    /* TODO: free more */
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
