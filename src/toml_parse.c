#include <ctype.h>

/**
 * This file is the root of the TOML parser for TOML configuration files.
 * TOML was chosen for its simplicity.
 */

#include "toml.h"

/* Get the column and line of @index within the active stream. */
static void get_stream_position(struct toml_parse_context *context,
        unsigned *line, unsigned *column)
{
    unsigned current_line = 0, current_column = 0;
    int character;

    rewind(context->file);

    while (character = fgetc(context->file), character != EOF &&
            ftell(context->file) <= context->item_index) {
        if (isprint(character)) {
            current_column++;
        } else if (character == '\n') {
            current_column = 0;
            current_line++;
        } else if (character == '\t') {
            current_column += 4;
        } else {
            /* these characters are printed as ? */
            current_column++;
        }
    }

    *line = current_line;
    *column = current_column;
}

/* Get the beginning of the line at given line index. */
static void print_line(struct toml_parse_context *context, unsigned line)
{
    int character;

    rewind(context->file);

    while (character = fgetc(context->file), !feof(context->file)) {
        if (character == '\n') {
            if (line == 0) {
                break;
            }
            line--;
        } else if (line == 0) {
            if (character == '\t') {
                puts("    ");
            } else if (!isprint(character)) {
                putchar('?');
            } else {
                putchar(character);
            }
        }
    }
}

/* Emit a parsing error. */
void emit_error(struct toml_parse_context *context, const char *format, ...)
{
    unsigned line, column;
    va_list list;

    get_stream_position(context, &line, &column);

    printf("%s:%d:%d: ", context->file_path, line + 1, column + 1);

    va_start(list, format);
    vprintf(format, list);
    va_end(list);

    printf("\n %4u | ", line + 1);
    print_line(context, line);
    putchar('\n');
    for (unsigned i = 0; i < column; i++) {
        putchar(' ');
    }
    puts("        ^\n");

    longjmp(context->jump, 1);
}

/* Skip over space which includes comments, space and tab. */
int skip_space(struct toml_parse_context *context, bool skip_new_lines)
{
    int character;

    while (context->item_index = ftell(context->file),
            character = fgetc(context->file), character != EOF) {
        if (isblank(character)) {
            continue;
        }

        /* skip over comments */
        if (character == '#') {
            while (character = fgetc(context->file),
                    character != EOF &&
                    character != '\n') {
                /* nothing */
            }
        }

        if (!isspace(character) || !skip_new_lines) {
            break;
        }
    }

    return character;
}

/* Parse the configuration in the TOML format. */
int parse_toml_configuration(const char *file_path, struct wm *wm)
{
    int character;
    int status = 0;
    struct toml_parse_context context;

    ZERO(&context, 1);

    context.file_path = file_path;
    context.file = fopen(file_path, "r");
    if (context.file == NULL) {
        status = 1;
    } else if (setjmp(context.jump) == 0) {
        /* parse the root table */
        parse_table(&context, false);

        /* read table headers and if found, parse the table content */
        while ((character = skip_space(&context, true)), character != EOF) {
            if (character == '[') {
                parse_table_header(&context);
                parse_table(&context, false);
            } else {
                emit_error(&context, "expected '[' for table start");
            }
        }
    } else {
        status = 1;
    }

    /* set the configuration or simply clear the build */
    if (status == 0) {
        *wm = context.wm;
    } else {
        clear_configuration(&context.wm);
    }

    free(context.string);

    if (context.file != NULL) {
        fclose(context.file);
    }

    return status;
}
