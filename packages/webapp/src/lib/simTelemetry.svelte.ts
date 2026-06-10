// The simulation-telemetry holder. Owns the display-only presentation fields
// derived purely from worker messages — generation/step/population counters,
// the survival-rate sparkline history, the active grid/steps parameters, and
// the snapshot-ready gate. It is the third sibling alongside `SimMachine`
// (owns { phase, dirty }) and `AgentFocus` (owns the focus triad).
//
// Unlike those two it issues no worker commands, so it takes no `send`
// callback. Worker replies call its `on*` intent methods; the view only reads
// its getters. No `$effect` lives inside the class — methods mutate state
// imperatively, keeping it unit-testable in isolation.
//
// The `on*` methods accept the relevant worker-reply payload as a single
// object, typed by these holder-owned structural shapes rather than the
// `WorkerEvent` union — so a narrowed `msg` satisfies them via structural
// subtyping (extra fields it carries are simply ignored) without coupling the
// holder to the worker wire protocol.

// The grid/steps parameters echoed by every (re)configuration reply.
type ConfigInfo = {
    population: number;
    gridSizeX: number;
    gridSizeY: number;
    stepsPerGen: number;
};

export class SimTelemetry {
    #gen = $state(0);
    #step = $state(0);
    #pop = $state(3000);
    #survivalHistory = $state<number[]>([]);

    // These reflect the active simulation parameters and are refreshed whenever
    // a (re)configuration reply arrives.
    #stepsPerGen = $state(300);
    #gridSizeX = $state(128);
    #gridSizeY = $state(128);

    // True once at least one generation boundary has been crossed
    // (snap.count > 0). Gates the snapshot-export affordance.
    #snapReady = $state(false);

    get gen(): number {
        return this.#gen;
    }

    get step(): number {
        return this.#step;
    }

    get pop(): number {
        return this.#pop;
    }

    get survivalHistory(): number[] {
        return this.#survivalHistory;
    }

    get stepsPerGen(): number {
        return this.#stepsPerGen;
    }

    get gridSizeX(): number {
        return this.#gridSizeX;
    }

    get gridSizeY(): number {
        return this.#gridSizeY;
    }

    get snapReady(): boolean {
        return this.#snapReady;
    }

    // Append a survival rate to the sparkline, capped at the most recent 12
    // entries. Guards a zero denominator (no population this generation).
    #pushSurvival(survivors: number, denom: number): void {
        const rate = denom > 0 ? survivors / denom : 0;
        this.#survivalHistory = [...this.#survivalHistory.slice(-11), rate];
    }

    // ── Worker-reply intents ──────────────────────────────────────────────────

    onStatus(e: { step: number }): void {
        this.#step = e.step;
    }

    onCensus(e: { gen: number; population: number; survivors: number }): void {
        this.#gen = e.gen;
        this.#pop = e.population;
        this.#pushSurvival(e.survivors, e.population);
        this.#snapReady = true;
    }

    onConfigured(e: ConfigInfo): void {
        this.#gen = 0;
        this.#step = 0;
        this.#survivalHistory = [];
        this.#pop = e.population;
        this.#gridSizeX = e.gridSizeX;
        this.#gridSizeY = e.gridSizeY;
        this.#stepsPerGen = e.stepsPerGen;
        this.#snapReady = false;
    }

    onRewindConfigured(e: ConfigInfo & { gen: number }): void {
        this.#gen = e.gen;
        this.#step = 0;
        this.#pop = e.population;
        this.#gridSizeX = e.gridSizeX;
        this.#gridSizeY = e.gridSizeY;
        this.#stepsPerGen = e.stepsPerGen;
        this.#survivalHistory = [];
    }

    onNextGenerationConfigured(
        e: ConfigInfo & {
            gen: number;
            censusPopulation: number;
            survivors: number;
        },
    ): void {
        this.#gen = e.gen;
        this.#step = 0;
        this.#pop = e.population;
        this.#gridSizeX = e.gridSizeX;
        this.#gridSizeY = e.gridSizeY;
        this.#stepsPerGen = e.stepsPerGen;
        this.#pushSurvival(e.survivors, e.censusPopulation);
        this.#snapReady = true;
    }

    onSnapshotLoaded(e: ConfigInfo & { gen: number }): void {
        this.#gen = e.gen;
        this.#step = 0;
        this.#pop = e.population;
        this.#gridSizeX = e.gridSizeX;
        this.#gridSizeY = e.gridSizeY;
        this.#stepsPerGen = e.stepsPerGen;
        this.#snapReady = true;
        this.#survivalHistory = [];
    }

    // ── User gestures ─────────────────────────────────────────────────────────

    // A manual next-gen / rewind without auto-play resets the step counter so
    // the HUD reads 0 until the worker reports the first step.
    resetStep(): void {
        this.#step = 0;
    }
}
