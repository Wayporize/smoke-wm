#include <locale.h>
#include <stdio.h>
#include <stdlib.h>

#include "binding.h"
#include "configuration.h"
#include "toml.h"
#include "x11.h"

/* make the linker happy */
char *program_name, *user_home;

int main(int argc, char **argv)
{
    uid_t user_id;
    char *path;

    (void) setlocale(LC_ALL, "");

    path = get_configuration_path();

    open_display();

    parse_toml_configuration(path, &Configuration);
    set_configuration_bindings(&Configuration);
    debug_dump_bindings();
    free(path);

    return 0;
}

