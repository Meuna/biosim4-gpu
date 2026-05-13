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
#else
#include <stdint.h>
#endif

/* 2D grid coordinate — split into separate SoA buffers for GPU, packed here for host use */
typedef struct {
    int16_t x;
    int16_t y;
} biosim_coord_t;

/* Grid cell sentinel values.
 * In OpenCL kernels the grid buffer is __global uint *; use (uint) casts so
 * comparisons are type-matched without implicit promotion warnings. */
#ifdef __OPENCL_VERSION__
#define BIOSIM_GRID_EMPTY   ((uint)0x0000U)
#define BIOSIM_GRID_BARRIER ((uint)0xFFFFU)
#else
#define BIOSIM_GRID_EMPTY   ((uint16_t)0x0000U)
#define BIOSIM_GRID_BARRIER ((uint16_t)0xFFFFU)
#endif

#endif /* BIOSIM_CORE_TYPES_H */
