/* biosim/core/gene.h — gene connectivity bit-layout macros.
 *
 * PORTABILITY: This header compiles under both C11 and OpenCL C.
 * Do NOT add <stdio.h>, <stdlib.h>, <string.h>, or any other host-only
 * include. OpenCL kernels include this file directly to decode conn_packed.
 */

#ifndef BIOSIM_CORE_GENE_H
#define BIOSIM_CORE_GENE_H

/* ── gene field encoding ─────────────────────────────────────────────────── */
/* conn layout: [srcType:1][srcNum:7][sinkType:1][sinkNum:7]
 *
 * Both srcType and sinkType share the same two values:
 *   BIOSIM_GENE_NEURON (0) — internal node; neuron as source or sink
 *   BIOSIM_GENE_IO     (1) — peripheral; sensor as source, action as sink
 *
 * Source/sink numbers are raw 7-bit values remapped modulo valid range
 * by the feedforward kernel.
 */

#define BIOSIM_GENE_NEURON 0U
#define BIOSIM_GENE_IO     1U

/* ── weight scaling ──────────────────────────────────────────────────────── */

/* int16_t gene weights are divided by this value to produce a float weight
 * in roughly ±4.0 range.  Must be shared between the host nnet module and
 * the OpenCL feedforward kernel so both produce byte-identical conversions. */
#define BIOSIM_GENE_WEIGHT_SCALE 8192.0F

#define BIOSIM_GENE_SRC_TYPE(c)  (((c) >> 15) & 1U)
#define BIOSIM_GENE_SRC_NUM(c)   (((c) >> 8) & 0x7FU)
#define BIOSIM_GENE_SINK_TYPE(c) (((c) >> 7) & 1U)
#define BIOSIM_GENE_SINK_NUM(c)  ((c) & 0x7FU)
#define BIOSIM_GENE_PACK(st, sn, dt, dn)                                                           \
    ((uint16_t)(((st) & 1U) << 15 | ((sn) & 0x7FU) << 8 | ((dt) & 1U) << 7 | ((dn) & 0x7FU)))

#endif /* BIOSIM_CORE_GENE_H */
