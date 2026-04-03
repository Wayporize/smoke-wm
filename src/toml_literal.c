#include <ctype.h>

/**
 * This file reads literal TOML values including booleans, integers and strings.
 * Float or date/time is not supported as it is not needed as of now.
 */

#include "toml.h"

/* Check if the character is part of a word or identifier. */
static bool is_word_character(int character)
{
    return isalnum(character) || character == '_' || character == '-';
}

/* Read a simple escape character. */
static void read_escape_character(struct toml_parse_context *context)
{
    long file_index;
    int character = 0;
    int digits = 0;
    wchar_t unicode = 0;
    int length;

    file_index = ftell(context->file);
    character = fgetc(context->file);
    switch (character) {
    case 'b': LIST_APPEND_VALUE(context->string, '\b'); break;
    case 'e': LIST_APPEND_VALUE(context->string, '\x1b'); break;
    case 'f': LIST_APPEND_VALUE(context->string, '\f'); break;
    case 'n': LIST_APPEND_VALUE(context->string, '\n'); break;
    case 'r': LIST_APPEND_VALUE(context->string, '\r'); break;
    case 't': LIST_APPEND_VALUE(context->string, '\t'); break;
    case 'x': digits = 2; break;
    case 'u': digits = 4; break;
    case 'U': digits = 8; break;
    case '\n': /* nothing */ break;
    case '\\': LIST_APPEND_VALUE(context->string, '\\'); break;

    default:
        context->item_index = file_index;
        emit_error(context, "invalid escape sequence");
    }

    for (int i = 0; i < digits; i++) {
        const int digit = fgetc(context->file);
        if (!isxdigit(digit)) {
            emit_error(context, "expected hexadecimal digit");
        }
        unicode *= 16;
        if (isdigit(digit)) {
            unicode += digit - '0';
        } else if (isupper(digit)) {
            unicode += digit - 'A';
        } else {
            unicode += digit - 'a';
        }
    }

    /* use `wctomb()` to convert unicode characters to UTF8 */
    if (digits > 0) {
        LIST_GROW(context->string, context->string_length + MB_CUR_MAX);
        length = wctomb(&context->string[context->string_length], unicode);
        if (length == -1) {
            context->item_index = file_index;
            emit_error(context, "invalid utf8 content within string");
        }
        context->string_length += length;
    }
}

/* Read a string quoted with ' or ". */
static void read_basic_string_quote(struct toml_parse_context *context,
        char quote)
{
    int character;

    context->string_length = 0;
    while (character = fgetc(context->file), true) {
        if (character == quote) {
            break;
        }

        if (character == EOF) {
            emit_error(context, "unexpected EOF, need %c to terminate string",
                    quote);
        }

        /* only interpret escape characters in "" strings */
        if (character == '\\' && quote == '\"') {
            read_escape_character(context);
        } else {
            LIST_APPEND_VALUE(context->string, character);
        }
    }
    LIST_APPEND_VALUE(context->string, '\0');
}

/* Read a basic string ".*" with escape characters */
void read_basic_string(struct toml_parse_context *context)
{
    read_basic_string_quote(context, '\"');
}

/* Read a literal string '.*'. */
void read_basic_literal_string(struct toml_parse_context *context)
{
    read_basic_string_quote(context, '\'');
}

static void read_string_quote(struct toml_parse_context *context, char quote)
{
    int character;
    bool is_multi_line = false;

    context->string_length = 0;
    character = fgetc(context->file);
    if (character == quote) {
        character = fgetc(context->file);
        if (character != quote) {
            ungetc(character, context->file);
            LIST_APPEND_VALUE(context->string, '\0');
            /* escape this lazy if mess */
            return;
        } else {
            is_multi_line = true;
            character = fgetc(context->file);
            /* ignore the first line break */
            if (character == '\n') {
                character = fgetc(context->file);
            }
        }
    }

    while (character != EOF) {
        if (character == quote) {
            if (!is_multi_line) {
                /* a normal string has ended */
                break;
            }

            /* check for "" */
            character = fgetc(context->file);
            if (character != quote) {
                /* no """, simply add the quote and continue with the read
                 * character
                 */
                LIST_APPEND_VALUE(context->string, quote);
            } else {
                /* check for """ */
                character = fgetc(context->file);
                if (character != quote) {
                    /* no """, simply add the quotes and continue with the read
                     * character
                     */
                    LIST_APPEND_VALUE(context->string, quote);
                    LIST_APPEND_VALUE(context->string, quote);
                } else {
                    /* got """, now read further " and append them */
                    while (character = fgetc(context->file),
                            character == '\"') {
                        LIST_APPEND_VALUE(context->string, quote);
                    }
                    ungetc(character, context->file);
                    /* multi line string has ended */
                    break;
                }
            }
        }

        /* only interpret escape characters in "" strings */
        if (character == '\\' && quote == '\"') {
            read_escape_character(context);
        } else {
            LIST_APPEND_VALUE(context->string, character);
        }

        character = fgetc(context->file);
    }

    if (character == EOF) {
        emit_error(context, "unexpected EOF, need string terminator");
    }

    LIST_APPEND_VALUE(context->string, '\0');
}

/* Read a string ".*" with escape characters or """.*""" with line breaks. */
void read_string(struct toml_parse_context *context)
{
    read_string_quote(context, '\"');
}

/* Read a literal string '.*' or '''.*''' with line breaks. */
void read_literal_string(struct toml_parse_context *context)
{
    read_string_quote(context, '\'');
}

/* Read a literal or doubly quoted string. */
void read_any_string(struct toml_parse_context *context)
{
    int character;

    character = skip_space(context, false);
    if (character == '\"' || character == '\'') {
        read_string_quote(context, character);
    } else {
        emit_error(context, "expected string");
    }
}

/* Read a word which is either a basic string or [A-Za-z0-9-_]+. */
void read_word(struct toml_parse_context *context)
{
    int character;

    character = skip_space(context, false);
    if (character == '\"' || character == '\'') {
        read_basic_string_quote(context, character);
    } else {
        context->string_length = 0;
        while (is_word_character(character)) {
            LIST_APPEND_VALUE(context->string, character);
            character = fgetc(context->file);
        }

        if (context->string_length == 0) {
            emit_error(context, "expected word");
        }

        LIST_APPEND_VALUE(context->string, '\0');
        ungetc(character, context->file);
    }
}

/* Read an integer according to given base character. */
static long read_integer_base_x(struct toml_parse_context *context,
        int character)
{
    long number = 0;

    if (character == 'x') {
        while (character = fgetc(context->file),
                isxdigit(character) || character == '_') {
            if (character == '_') {
                continue;
            }
            number *= 16;
            if (isdigit(character)) {
                number += character - '0';
            } else if (isupper(character)) {
                number += character - 'A';
            } else {
                number += character - 'a';
            }
        }
    } else if (character == 'o') {
        while (character = fgetc(context->file),
                (character >= '0' && character <= '7') ||
                character == '_') {
            if (character == '_') {
                continue;
            }
            number *= 8;
            number += character - '0';
        }
    } else if (character == 'b') {
        while (character = fgetc(context->file),
                character == '0' || character == '1' ||
                character == '_') {
            if (character == '_') {
                continue;
            }
            number *= 2;
            number += character - '0';
        }
    }

    ungetc(character, context->file);

    return number;
}

/* Read an integer and put it into @context->number. */
void read_integer(struct toml_parse_context *context)
{
    int character;
    long sign = 1;
    long number = 0;

    character = skip_space(context, false);

    /* 0[xob][0-9a-fA-F_]+ */
    if (character == '0') {
        character = fgetc(context->file);
        number = read_integer_base_x(context, character);
    } else {
        if (!isdigit(character)) {
            if (character == '-') {
                sign = -1;
            }
            /* [0-9] */
            character = fgetc(context->file);
            if (!isdigit(character)) {
                emit_error(context, "expected digit after sign");
            }
        }

        number = character - '0';

        /* [0-9_]* */
        while (character = fgetc(context->file),
                isdigit(character) || character == '_') {
            if (character == '_') {
                continue;
            }
            number *= 10;
            number += character - '0';
        }

        ungetc(character, context->file);
    }

    context->number = sign * number;
}

/* Read a boolean (true or false) and put it into @context->number. */
void read_boolean(struct toml_parse_context *context)
{
    int character;

    /* read a "true" or "false" */
    character = skip_space(context, false);
    if (character == 't') { /* t */
        if (fgetc(context->file) != 'r' || /* tr */
                fgetc(context->file) != 'u' || /* tru */
                fgetc(context->file) != 'e' || /* true */
                /* make sure it is a word ending */
                is_word_character(character = fgetc(context->file))) {
            emit_error(context, "expected true or false");
        }
        context->number = true;
    } else if (character == 'f') { /* f */
        if (fgetc(context->file) != 'a' || /* fa */
                fgetc(context->file) != 'l' || /* fal */
                fgetc(context->file) != 's' || /* fals */
                fgetc(context->file) != 'e' || /* false */
                /* make sure it is a word ending */
                is_word_character(character = fgetc(context->file))) {
            emit_error(context, "expected true or false");
        }
        context->number = false;
    } else {
        emit_error(context, "expected true or false");
    }
    /* we read one character too much */
    ungetc(character, context->file);
}
