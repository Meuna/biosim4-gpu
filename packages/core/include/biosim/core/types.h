/*
 * HOST/DEVICE PORTABILITY: this header is included by OpenCL kernel sources.
 * Do NOT add <stdio.h>, <stdlib.h>, <string.h>, or any other host-only header.
 * OpenCL C has no <stdint.h>; the guard below provides compatible typedefs.
 */
#ifndef BIOSIM_CORE_TYPES_H
#define BIOSIM_CORE_TYPES_H

#ifdef __OPENCL_VERSION__
typedef short int16_t;
typedef ushort uint16_t;
typedef int int32_t;
typedef uint uint32_t;
#else
#include <stdint.h>
#endif

/* 2D grid coordinate — split into separate SoA buffers for GPU, packed here for host use */
typedef struct {
    int32_t x;
    int32_t y;
} biosim_coord_t;

/* Grid cell sentinel values.
 * Cell encoding: BIOSIM_GRID_EMPTY (0), BIOSIM_GRID_BARRIER (0xFFFFFFFF),
 * or a 1-based agent index in [1, 0xFFFFFFFE]. */
#ifdef __OPENCL_VERSION__
#define BIOSIM_GRID_EMPTY   ((uint)0x00000000U)
#define BIOSIM_GRID_BARRIER ((uint)0xFFFFFFFFU)
#else
#define BIOSIM_GRID_EMPTY   ((uint32_t)0x00000000U)
#define BIOSIM_GRID_BARRIER ((uint32_t)0xFFFFFFFFU)
#endif

#endif /* BIOSIM_CORE_TYPES_H */
