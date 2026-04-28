#ifndef LOG_H
#define LOG_H

/* Output formatted to stdout.
 *
 * A line ending is inserted automatically.
 *
 * Each line is prefixed with the current time.
 */
void notef(const char *format, ...);

#endif

