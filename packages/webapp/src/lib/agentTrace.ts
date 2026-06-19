// Point-management for the selected-agent trace (the path drawn behind the
// agent the user is inspecting). The worker records one grid cell per sim step;
// the drawing itself lives in sim.worker.ts.

export interface TracePoint {
    gx: number;
    gy: number;
}

// Hard cap on retained points. A generation is at most a few hundred steps, so
// this only guards against pathological configs; it never bites in practice.
export const MAX_TRACE_POINTS = 512;

// Append a grid cell to the trace, skipping consecutive duplicates (the agent
// stayed put this step) and capping length by dropping the oldest. Mutates and
// returns `trace` so the worker's per-step hot path allocates nothing.
export function appendTracePoint(
    trace: TracePoint[],
    gx: number,
    gy: number,
    max = MAX_TRACE_POINTS,
): TracePoint[] {
    const last = trace[trace.length - 1];
    if (last && last.gx === gx && last.gy === gy) return trace;
    trace.push({ gx, gy });
    if (trace.length > max) trace.shift();
    return trace;
}
