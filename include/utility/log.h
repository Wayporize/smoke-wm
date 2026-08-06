#ifndef LOG_H
#define LOG_H

/* Output formatted to stdout.
 *
 * Each line is prefixed with the current time.
 */
void notef(const char *format, ...);

/* Alias for `notef`. */
#define LOG notef

#endif

