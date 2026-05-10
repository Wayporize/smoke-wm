#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include <xkbcommon/xkbcommon.h>

/**
 * This file handles the management of configuration objects and related parts
 * of the configuration like the configuration directory.
 */

#include "binding.h"
#include "configuration.h"
#include "display.h"

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

#ifdef DEBUG

/* Print the given color as four lines to stdout and prefix each with @prefix.
 */
static void print_color(const char *prefix, xcb_render_color_t *color)
{
    printf("%s.red = 0x%04x\n", prefix, color->red);
    printf("%s.green = 0x%04x\n", prefix, color->green);
    printf("%s.blue = 0x%04x\n", prefix, color->blue);
    printf("%s.alpha = 0x%04x\n", prefix, color->alpha);
}

/* Print a border in multiple lines to stdout. */
static void print_border(struct wm_border *border)
{
    printf("border.size = %d\n", border->size);
    printf("border.decoration = ");
    switch (border->decoration) {
    case BORDER_UNSPECIFIED: printf("unspecified"); break;
    case BORDER_NONE: printf("none"); break;
    case BORDER_SIMPLE: printf("simple"); break;
    case BORDER_FULL: printf("full"); break;
    }
    printf("\n");
    printf("border.radius.inner = %d\n", border->radius.inner);
    printf("border.radius.outer = %d\n", border->radius.outer);
    print_color("border.color.focused", &border->color.focused);
    print_color("border.color.highlight", &border->color.highlight);
    print_color("border.color.inactive", &border->color.inactive);
    print_color("border.color.floating", &border->color.floating);
    print_color("border.color.tiling", &border->color.tiling);
}

/* Print a layout constant to stdout. */
static void print_layout(enum tiling_layout layout)
{
    printf("layout = ");
    switch (layout) {
    case TILE_UNSPECIFIED: printf("unspecified"); break;
    case TILE_AUTO: printf("auto"); break;
    case TILE_STACK: printf("stack"); break;
    case TILE_HORIZONTAL: printf("horizontal"); break;
    case TILE_VERTICAL: printf("vertical"); break;
    case TILE_GRID: printf("grid"); break;
    case TILE_SPIRAL: printf("spiral"); break;
    }
    printf("\n");
}

/* Dump all configuration options to stdout. */
void debug_dump_configuration(struct wm *wm)
{
    printf("tiling.");
    print_layout(wm->tiling.layout);
    for (int i = 0; i < 4; i++) {
        printf("tiling.gaps.inner[%d] = %d\n", i, wm->tiling.gaps.inner[i]);
    }
    for (int i = 0; i < 4; i++) {
        printf("tiling.gaps.outer[%d] = %d\n", i, wm->tiling.gaps.outer[i]);
    }

    print_border(&wm->border);

    for (size_t i = 0; i < wm->monitor_length; i++) {
        printf("[[MONITOR]]\n");
        printf("name = %s\n", wm->monitor[i].name);
        print_layout(wm->monitor[i].layout);
    }

    for (size_t i = 0; i < wm->workspace_length; i++) {
        printf("[[WORKSPACE]]\n");
        printf("name = %s\n", wm->workspace[i].name);
        printf("number = %u\n", wm->workspace[i].number);
        printf("monitor = %s\n", wm->workspace[i].monitor);
        print_layout(wm->workspace[i].layout);
    }

    for (size_t i = 0; i < wm->window_length; i++) {
        printf("[[WINDOW]]\n");
        printf("name = %s\n", wm->window[i].name);
        printf("class = %s\n", wm->window[i].class);
        printf("instance = %s\n", wm->window[i].instance);
        printf("workspace = %s\n", wm->window[i].workspace);
        printf("monitor = %s\n", wm->window[i].monitor);
        printf("hidden = %d\n", wm->window[i].hidden);
        printf("mode = ");
        switch (wm->window[i].mode) {
        case WINDOW_UNSPECIFIED: printf("unspecified"); break;
        case WINDOW_TILING: printf("tiling"); break;
        case WINDOW_FLOATING: printf("floating"); break;
        case WINDOW_FULLSCREEN: printf("fullscreen"); break;
        }
        printf("\n");
        print_border(&wm->window[i].border);
    }

    for (size_t i = 0; i < wm->binding_length; i++) {
        printf("[[BINDING]]\n");
        printf("release = %d\n", wm->binding[i].is_release);
        printf("transparent = %d\n", wm->binding[i].is_transparent);
        printf("modifiers = %u\n", wm->binding[i].modifiers);
        printf("key_symbol = %u\n", wm->binding[i].key_symbol);
        printf("key_code = %u\n", wm->binding[i].key_code);
        printf("button = %u\n", wm->binding[i].button);
        printf("TODO: action, value\n");
    }

    for (size_t i = 0; i < wm->startup_length; i++) {
        printf("[[STARTUP]]\n");
        printf("TODO: action, value\n");
    }
}

#endif

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

/* Set the bindings of a configuration as global bindings. */
void set_configuration_bindings(struct wm *wm)
{
    for (size_t i = 0; i < wm->binding_length; i++) {
        struct action action;

        action.type = wm->binding[i].action;
        action.value = wm->binding[i].value;
        if (wm->binding[i].button != 0) {
            append_button_binding(wm->binding[i].is_release,
                    wm->binding[i].is_transparent,
                    wm->binding[i].modifiers, wm->binding[i].button - 1,
                    action);
        }

        if (wm->binding[i].key_code != XKB_KEY_NoSymbol) {
            append_key_binding(wm->binding[i].is_release,
                    wm->binding[i].modifiers, wm->binding[i].key_code,
                    action);
        }

        if (wm->binding[i].key_symbol != XKB_KEY_NoSymbol) {
            append_key_symbol_binding(wm->binding[i].is_release,
                    wm->binding[i].modifiers, wm->binding[i].key_symbol,
                    action);
        }
    }
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
    const char *const config = "smoke-wm/config.toml";
    const char *xdg_config_home, *xdg_config_dirs;
    char *path;
    const char *colon;
    int length;

    /* search for the configuration file smoke-wm/config.toml within these
     * directories (ordered by preference):
     * XDG_CONFIG_HOME:XDG_CONFIG_DIRS:~/.config:/etc/xdg
     */

    xdg_config_home = getenv("XDG_CONFIG_HOME");
    if (xdg_config_home != NULL && xdg_config_home[0] != '\0') {
        path = xasprintf("%s/%s", xdg_config_home, config);
        if (is_readable(path)) {
            return path;
        }
        free(path);
    }

    xdg_config_dirs = getenv("XDG_CONFIG_DIRS");
    if (xdg_config_dirs != NULL && xdg_config_dirs[0] != '\0') {
        do {
            colon = strchr(xdg_config_dirs, ':');
            if (colon != NULL) {
                length = colon - xdg_config_dirs;
            } else {
                length = strlen(xdg_config_dirs);
            }

            path = xasprintf("%.*s/%s",
                    length, xdg_config_dirs, config);
            if (is_readable(path)) {
                return path;
            }
            free(path);

            xdg_config_dirs = colon + 1;
        } while (colon != NULL);
    }

    path = xasprintf("%s/.config/%s", user_home, config);
    if (is_readable(path)) {
        return path;
    }
    free(path);

    path = xasprintf("/etc/xdg/%s", config);
    if (is_readable(path)) {
        return path;
    }
    free(path);

    return NULL;
}
