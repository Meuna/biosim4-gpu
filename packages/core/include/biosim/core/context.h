#ifndef BIOSIM_CORE_CONTEXT_H
#define BIOSIM_CORE_CONTEXT_H

/*
 * Simulation configuration passed to core algorithms by the simulator.
 * Populated from biosim_params_t before and during the simulation loop.
 */
typedef struct {
    int steps_per_gen;
    int population_sensor_radius;
} biosim_context_t;

#endif /* BIOSIM_CORE_CONTEXT_H */
