#ifndef TOML_H

#include <setjmp.h>
#include <stdio.h>

/**
 * TOML parser without validity checking for duplicate tables.
 * Float/Date/Time is also not supported.
 */

#include "configuration.h"

/* context needed to parse a file */
struct toml_parse_context {
    /* the file being read from */
    const char *file_path;
    FILE *file;

    /* when an error occurs, all functions are interrupted and we end up in the
     * main parsing function
     */
    jmp_buf jump;

    /* start of the current item */
    long item_index;

    /* index of the current table, see toml_table_and_key.c */
    unsigned table;

    /* current configuration being built */
    struct wm wm;

    /* utf8 encoded string */
    LIST(char, string);
    /* value of parsed number */
    long number;
};

/* Emit a parsing error.
 *
 * This will cancel parsing, any code below it is unreachable.
 */
void emit_error(struct toml_parse_context *context, const char *format, ...);

/* Skip over space which includes comments, space and tab.
 *
 * @skip_new_lines controls whether new lines should be skipped too.
 */
int skip_space(struct toml_parse_context *context, bool skip_new_lines);

/* Read a basic string (").*" with escape characters. */
void read_basic_string(struct toml_parse_context *context);

/* Read a literal string (').*'. */
void read_basic_literal_string(struct toml_parse_context *context);

/* Read a string (").*" with escape characters or (")"".*""" with line breaks.
 */
void read_string(struct toml_parse_context *context);

/* Read a literal string (').*' or (')''.*'''. */
void read_literal_string(struct toml_parse_context *context);

/* Read a literal or doubly quoted string. */
void read_any_string(struct toml_parse_context *context);

/* Read a word which is either a basic string or [A-Za-z0-9-_]+. */
void read_word(struct toml_parse_context *context);

/* Read an integer and put it into @context->number. */
void read_integer(struct toml_parse_context *context);

/* Read a boolean (true or false) and put it into @context->number. */
void read_boolean(struct toml_parse_context *context);

/* Parse keys within the root of the TOML document. */
void parse_root_key(struct toml_parse_context *context);

/* Parse a table header [[?word(.word)*]?] and move into the table. */
void parse_table_header(struct toml_parse_context *context);

/* Parse the content of a table.
 *
 * If @is_inline is true, then the table is defined as follows:
 * table_name = { table_content separated by , }
 *
 * If @is_inline is false, it looks like:
 * [table_name]
 * table_content separated by new lines
 */
void parse_table(struct toml_parse_context *context, bool is_inline);

/* Parse the configuration in the TOML format. */
int parse_toml_configuration(const char *file_path, struct wm *wm);

#endif
