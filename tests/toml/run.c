#include <locale.h>

#include "toml.h"

/* define this because the linker needs it */
char *user_home;

int main(int argc, char **argv)
{
    struct wm wm;
    int status;

    /* make wctomb() work */
    (void) setlocale(LC_ALL, "");

    status = parse_toml_configuration(argv[1], &wm);
    if (status == 0) {
        clear_configuration(&wm);
        return 0;
    }
    return 1;
}
