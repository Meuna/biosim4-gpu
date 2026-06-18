// Pure predicate for the idle-timeout return to the kinematic sculpture.
//
// The worker drives the grid render loop; when the simulation has been at rest
// (not playing, not free-running, and showing the grid) for longer than the
// timeout, it dissolves the grid back into the sculpture. Keeping the decision
// pure lets us unit-test the branching without the canvas/WASM coupling that
// makes the worker itself untestable.

export type RenderLoopMode =
    | "idle"
    | "transitioning-in"
    | "transitioning-out"
    | "running";

export interface IdleReturnArgs {
    mode: RenderLoopMode;
    playing: boolean;
    freeRunning: boolean;
    now: number;
    lastActivity: number;
    timeoutMs: number;
}

// True only when the grid is shown ("running"), the sim is fully at rest, and
// the rest has lasted longer than timeoutMs. Active play/free-run never drifts.
export function shouldIdleReturn(args: IdleReturnArgs): boolean {
    const { mode, playing, freeRunning, now, lastActivity, timeoutMs } = args;
    if (mode !== "running") return false;
    if (playing || freeRunning) return false;
    return now - lastActivity > timeoutMs;
}
