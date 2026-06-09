export const SIM_STATES = [
    "WORKER_PENDING",
    "WORKER_READY",
    "GENERATION_SPAWNED",
    "STEPS_RUNNING",
    "STEPS_PAUSED",
    "GENERATION_ENDED",
    "FREE_RUNNING",
    "FREE_RUN_STOPPING",
    "DIRTY_GENERATION_SPAWNED",
    "DIRTY_STEPS_PAUSED",
    "DIRTY_STEPS_RUNNING",
    "DIRTY_GENERATION_ENDED",
    "DIRTY_CONFIRM",
    "DIRTY_FREE_RUNNING",
    "DIRTY_FREE_RUN_STOPPING",
] as const;

export type SimState = (typeof SIM_STATES)[number];

// Ask the machine to enter dirty mode. Returns the DIRTY_* equivalent of the
// current state, or the state unchanged if no dirty variant exists (e.g.
// WORKER_PENDING, WORKER_READY, or any state already in DIRTY_*).
export function enterDirty(s: SimState): SimState {
    switch (s) {
        case "GENERATION_SPAWNED":
            return "DIRTY_GENERATION_SPAWNED";
        case "STEPS_RUNNING":
            return "DIRTY_STEPS_RUNNING";
        case "STEPS_PAUSED":
            return "DIRTY_STEPS_PAUSED";
        case "GENERATION_ENDED":
            return "DIRTY_GENERATION_ENDED";
        case "FREE_RUNNING":
            return "DIRTY_FREE_RUNNING";
        case "FREE_RUN_STOPPING":
            return "DIRTY_FREE_RUN_STOPPING";
        default:
            return s;
    }
}

// Ask the machine to exit dirty mode. Returns the clean equivalent of the
// current state, or the state unchanged when no automatic exit applies.
// DIRTY_CONFIRM is not automatically exited — only an explicit user action can
// clear it.
export function exitDirty(s: SimState): SimState {
    switch (s) {
        case "DIRTY_GENERATION_SPAWNED":
            return "GENERATION_SPAWNED";
        case "DIRTY_STEPS_RUNNING":
            return "STEPS_RUNNING";
        case "DIRTY_STEPS_PAUSED":
            return "STEPS_PAUSED";
        case "DIRTY_GENERATION_ENDED":
            return "GENERATION_ENDED";
        case "DIRTY_FREE_RUNNING":
            return "FREE_RUNNING";
        case "DIRTY_FREE_RUN_STOPPING":
            return "FREE_RUN_STOPPING";
        default:
            return s;
    }
}

// ── Worker-event transitions ─────────────────────────────────────────────────

export function onWorkerReady(s: SimState): SimState {
    return s === "WORKER_PENDING" ? "WORKER_READY" : s;
}

// The worker has finished configuring a new generation. After this the
// isDirty sync effect will re-evaluate and may transition to a DIRTY_* variant
// if the draft still differs from what was just applied.
export function onConfigured(): SimState {
    return "GENERATION_SPAWNED";
}

export function onGenComplete(s: SimState): SimState {
    switch (s) {
        case "STEPS_RUNNING":
            return "GENERATION_ENDED";
        case "DIRTY_STEPS_RUNNING":
            return "DIRTY_GENERATION_ENDED";
        default:
            return s;
    }
}

// Called when the worker reports "paused" after a stopFreeRun command.
export function onFreeRunPaused(s: SimState): SimState {
    switch (s) {
        case "FREE_RUN_STOPPING":
            return "STEPS_PAUSED";
        case "DIRTY_FREE_RUN_STOPPING":
            return "DIRTY_STEPS_PAUSED";
        default:
            return s;
    }
}

// Called when "rewindConfigured" or "nextGenerationConfigured" arrives.
// If already running (autoPlay was sent before the reply), the running state
// is preserved.
export function onRewindOrNextGenDone(s: SimState): SimState {
    switch (s) {
        case "GENERATION_ENDED":
            return "GENERATION_SPAWNED";
        case "DIRTY_GENERATION_ENDED":
            return "DIRTY_GENERATION_SPAWNED";
        case "DIRTY_CONFIRM":
            // Rewind issued from the config-change dialog: the pending config
            // is applied, so the result is a clean spawn.
            return "GENERATION_SPAWNED";
        default:
            return s;
    }
}

export function onSnapshotLoaded(): SimState {
    return "STEPS_PAUSED";
}
