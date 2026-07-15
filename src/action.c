#include <ctype.h>
#include <utility/log.h>
#include <utility/utility.h>

#include "action.h"

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

/* Get the data type a specific action requires. */
enum action_data_type get_data_type_of_action_type(enum action_type type)
{
    return action_specifications[type].data_type;
}

/* Execute a user action. */
void execute_action(const struct action *action)
{
    LOG("running action %s", action_specifications[action->type].name);
    switch (get_data_type_of_action_type(action->type)) {
    case ACTION_DATA_NULL:
        UNREACHABLE;
    case ACTION_DATA_VOID:
        /* nothing to print */
        break;
    case ACTION_DATA_INTEGER:
        printf(": %d", action->value.integer);
        break;
    case ACTION_DATA_STRING:
        printf(": %s", action->value.string);
        break;
    }
    printf("\n");
}

