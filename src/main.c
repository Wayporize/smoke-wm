#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <pwd.h>
#include <sys/types.h>
#include <utility/utility.h>

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
            -v, --version       Show the version\n");
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

    (void) setlocale(LC_ALL, "");

    /* get the user id and refuse to run as root */
    user_id = getuid();
    if (user_id == 0) {
        printf("smoke-wm is not allowed to be run as root user\n");
        exit(EXIT_FAILURE);
    }

    /* get the home directory through HOME or fall back to the passwd entry */
    home = getenv("HOME");
    if (home == NULL || home[0] == '\0') {
        passwd = getpwuid(user_id);
        user_home = xstrdup(passwd->pw_dir);
        printf("HOME not set, falling back to passwd entry: %s\n",
                passwd->pw_dir);
    } else {
        user_home = xstrdup(home);
    }

    printf("user home: %s\n",
            user_home);

    /* TODO: read configuration from the config directory or
     * /usr/share/smoke-wm/config.toml if not present
     */

    return 0;
}
