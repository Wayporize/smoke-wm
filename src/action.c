#include <ctype.h>
#include <utility/log.h>
#include <utility/utility.h>

#include "action.h"
#include "monitor.h"

static const struct action_specification {
    const char *name;
    enum action_type type;
    enum action_data_type data_type;
} action_specifications[] = {
    { NULL, ACTION_NULL, ACTION_DATA_NULL },
#define X(name, type) { STRINGIFY(name), ACTION_##name, ACTION_DATA_##type },
    DECLARE_ALL_ACTIONS
#undef X
};

/* Get the action type corresponding to given string. */
enum action_type convert_string_to_action_type(const char *string)
{
    char upper[16];
    char *u;
    for (u = upper; string[0] != '\0'; u++) {
        if (u >= upper + sizeof(upper)) {
            return ACTION_NULL;
        }
        u[0] = toupper(string[0]);
        string++;
    }
    u[0] = '\0';
    for (size_t i = 1; i < SIZE(action_specifications); i++) {
        if (strcmp(upper, action_specifications[i].name) == 0) {
            return (enum action_type) i;
        }
    }
    return ACTION_NULL;
}

/* Get a string representation of an action type. */
const char *get_string_of_action_type(enum action_type type)
{
    return action_specifications[type].name;
}

/* Get the data type a specific action requires. */
enum action_data_type get_data_type_of_action_type(enum action_type type)
{
    return action_specifications[type].data_type;
}

/* Print the action value to stdout. */
void print_action_value(enum action_type type, union action_value value, const char *prefix)
{
    switch (get_data_type_of_action_type(type)) {
    case ACTION_DATA_NULL:
    case ACTION_DATA_VOID:
        /* nothing to print */
        break;
    case ACTION_DATA_INTEGER:
        printf("%s%d", prefix, value.integer);
        break;
    case ACTION_DATA_STRING:
        printf("%s%s", prefix, value.string);
        break;
    }
}

static void NONE(union action_value value)
{
    (void) value;
}

static void WORKSPACE(union action_value value)
{
    focus_workspace(value.string);
}

static void FOCUS(union action_value value)
{
    (void) value;
    /* TODO: */
}

static void CLOSE(union action_value value)
{
    (void) value;
    /* TODO: */
}

static void RUN(union action_value value)
{
    run_shell(value.string);
}

/* Execute a user action. */
void execute_action(const struct action *action)
{
    LOG("running action %s", action_specifications[action->type].name);
    print_action_value(action->type, action->value, ": ");
    printf("\n");
    switch (action->type) {
    case ACTION_NULL: /* do the same as NONE */
#define X(name, type) case ACTION_##name: name(action->value); break;
    DECLARE_ALL_ACTIONS
#undef X
    }
}
