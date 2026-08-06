#ifndef UTILITY__TYPES_H
#define UTILITY__TYPES_H

/**
 * Helpful data types.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* byte of a utf8 sequence */
typedef char utf8_t;

/* a point at position x, y */
struct position {
    /* horizontal position */
    int32_t x;
    /* vertical position */
    int32_t y;
};

/* a size of width x height */
struct size {
    /* horizontal size */
    int32_t width;
    /* vertical size */
    int32_t height;
};

/* offsets from the edges of *something* */
struct extents {
    /* left extent */
    int32_t left;
    /* right extent */
    int32_t right;
    /* top extent */
    int32_t top;
    /* bottom extent */
    int32_t bottom;
};

/* a rectangular region */
struct rectangle {
    /* horizontal position */
    int32_t x;
    /* vertical position */
    int32_t y;
    /* horizontal size */
    int32_t width;
    /* vertical size */
    int32_t height;
};

/* fraction: numerator over denominator */
struct ratio {
    int32_t numerator;
    int32_t denominator;
};

#endif
