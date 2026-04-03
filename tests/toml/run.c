#include <locale.h>

#include "toml.h"

int main(int argc, char **argv)
{
    struct wm wm;

    /* make wctomb() work */
    (void) setlocale(LC_ALL, "");

    return parse_toml_configuration(argv[1], &wm);
}
