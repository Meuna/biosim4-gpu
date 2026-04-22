/*
 * HOST-ONLY: references heap-pointer structs from core.
 * Do NOT include from OpenCL kernel sources (.cl files).
 */
#ifndef BIOSIM_STEPPER_STEP_H
#define BIOSIM_STEPPER_STEP_H

#include "biosim/core/agents.h"
#include "biosim/core/genome.h"
#include "biosim/core/grid.h"
#include "biosim/core/nnet.h"
#include "biosim/core/params.h"
#include "biosim/core/status.h"
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const biosim_params_t *params; /* borrowed — caller owns, must outlive stepper */
    biosim_agents_t agents;
    biosim_grid_t grid;
    biosim_genome_t genome;
    biosim_nnet_t nnet;
    uint32_t *signal;  /* flat [size_y * size_x], row-major, values clamped [0, 255] */
    size_t signal_len; /* cached size_x * size_y */
    uint32_t step;     /* step index within the current generation */
} biosim_stepper_t;

biosim_status_t biosim_stepper_create(biosim_stepper_t *out, const biosim_params_t *params);
void biosim_stepper_free(biosim_stepper_t *stepper);
void biosim_stepper_step(biosim_stepper_t *stepper);

#endif /* BIOSIM_STEPPER_STEP_H */
