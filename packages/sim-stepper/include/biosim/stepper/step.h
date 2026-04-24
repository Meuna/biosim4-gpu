/*
 * HOST-ONLY: references heap-pointer structs from core.
 * Do NOT include from OpenCL kernel sources (.cl files).
 */
#ifndef BIOSIM_STEPPER_STEP_H
#define BIOSIM_STEPPER_STEP_H

#include "biosim/core/context.h"
#include "biosim/core/status.h"
#include "biosim/params/params.h"
#include <stdint.h>

/*
 * biosim_stepper_t extends biosim_context_t via first-member embedding.
 * A biosim_stepper_t * can be safely cast to biosim_context_t * because C
 * guarantees the first member is at offset 0.
 */
typedef struct {
    biosim_context_t base; /* FIRST — offset 0, safe up-cast to biosim_context_t * */
    uint32_t step;         /* step index within the current generation */
} biosim_stepper_t;

biosim_status_t biosim_stepper_create(biosim_stepper_t *out, const biosim_params_t *params);
void biosim_stepper_free(biosim_stepper_t *stepper);
void biosim_stepper_step(biosim_stepper_t *stepper);

#endif /* BIOSIM_STEPPER_STEP_H */
