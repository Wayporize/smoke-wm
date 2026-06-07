#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <pwd.h>
#include <sys/types.h>
#include <utility/log.h>
#include <utility/utility.h>

#include "binding.h"
#include "configuration.h"
#include "display.h"
#include "toml.h"

/* name of the executable argument used when running the program */
char *program_name;

/* home directory of the user */
char *user_home;

/* Show a quick description and the program arguments. */
void show_usage(int exit_code)
{
    puts("smoke-wm is a robust and modern X11 tiling window manager\n");
    printf("Usage: %s [OPTIONS...]\n",
            program_name);
    puts("Options:\n\
            -h, --help, --usage Show this help\n\
            -v, --version       Show the version");
    exit(exit_code);
}

/* Show the version number and versions of used libraries. */
void show_version(void)
{
    puts("smoke-wm version 0.1\n");
    exit(EXIT_SUCCESS);
}

/* smoke-wm main entry point. */
int main(int argc, char **argv)
{
    uid_t user_id;
    const char *home;
    struct passwd *passwd;
    char *path;
    enum wm_ownership_status status;

    (void) setlocale(LC_ALL, "");

#ifdef DEBUG
    /* make stdout line buffered */
    setvbuf(stdout, NULL, _IOLBF, 0);
#endif

    /* store the first argument containing the executable */
    program_name = argv[0];

    /* check for specific program arguments */
    for (argc--, argv++; argc > 0; argc--, argv++) {
        if (strcmp(argv[0], "-h") == 0 ||
                strcmp(argv[0], "--help") == 0 ||
                strcmp(argv[0], "--usage") == 0) {
            show_usage(EXIT_SUCCESS);
        } else if (strcmp(argv[0], "-v") == 0 ||
                strcmp(argv[0], "--version") == 0) {
            show_version();
        } else {
            show_usage(EXIT_FAILURE);
        }
    }

    /* get the user id and refuse to run as root */
    user_id = getuid();
    ASSERT(user_id != 0, "smoke-wm is not allowed to be run as root user");

    /* get the home directory through HOME or fall back to the passwd entry */
    home = getenv("HOME");
    if (home == NULL || home[0] == '\0') {
        passwd = getpwuid(user_id);
        user_home = xstrdup(passwd->pw_dir);
        notef("HOME not set, falling back to passwd entry: %s\n",
                passwd->pw_dir);
    } else {
        user_home = xstrdup(home);
    }

    /* associated to test "home" */
    notef("user home: %s\n", user_home);

    path = get_configuration_path();

    /* associated to test "configuration-path" */
    notef("configuration path: %s\n", path);

    open_display();

    /* parse the configuration or set the default one */
    if (path != NULL) {
        if (parse_toml_configuration(path, &Configuration) == 0) {
            /* associated to test "toml" */
            notef("parsing configuration succeeded\n");
            set_configuration_bindings(&Configuration);
            report_configuration_change_to_workspaces(&Configuration);
        } else {
            /* associated to test "toml" */
            notef("parsing configuration failed\n");
            Configuration = Configuration_default;
            /* TODO: set default bindings */
        }
        free(path);
    } else {
        notef("no configuration, using default\n");
        Configuration = Configuration_default;
        /* TODO: set default bindings */
    }

    /* associated to test "configuration" */
    notef("start of dumping configuration\n");
    dump_configuration(&Configuration);
    notef("end of dumping configuration\n");

    /* associated to test "bindings" */
    notef("start of dumping bindings\n");
    dump_bindings();
    notef("end of dumping bindings\n");

    /* try to become the active window manager */
    status = take_wm_ownership();
    ASSERT(status == WM_OWNERSHIP_SUCCESS, "could not become the window manager");

    /* receive all events by the server and handle them */
    handle_server_events();

    return 0;
}
