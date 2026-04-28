#include <stdarg.h>
#include <stdio.h>
#include <time.h>

/* Output formatted to stdout. */
void notef(const char *format, ...)
{
    struct timespec current_time;
    va_list list;

    clock_gettime(CLOCK_MONOTONIC, &current_time);
    printf("[%6lu.%06lu] ", current_time.tv_sec,
            current_time.tv_nsec / 1000);
    va_start(list, format);
    vprintf(format, list);
    va_end(list);
}
