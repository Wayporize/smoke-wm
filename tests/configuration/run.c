#include <locale.h>
#include <stdio.h>

#include "configuration.h"
#include "toml.h"

static void print_color(const char *prefix, xcb_render_color_t *color)
{
    printf("%s.red = 0x%04x\n", prefix, color->red);
    printf("%s.green = 0x%04x\n", prefix, color->green);
    printf("%s.blue = 0x%04x\n", prefix, color->blue);
    printf("%s.alpha = 0x%04x\n", prefix, color->alpha);
}

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

void print_layout(enum tiling_layout layout)
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

static void print_configuration(struct wm *wm)
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

int main(int argc, char **argv)
{
    struct wm wm;
    int status;

    /* make wctomb() work */
    (void) setlocale(LC_ALL, "");

    status = parse_toml_configuration(argv[1], &wm);
    if (status == 0) {
        print_configuration(&wm);
        clear_configuration(&wm);
        return 0;
    }
    return 1;
}
