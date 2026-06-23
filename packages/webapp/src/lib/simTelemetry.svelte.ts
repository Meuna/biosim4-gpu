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
    #kills = $state(0);
    #survivalHistory = $state<number[]>([]);

    // Running aggregates over the full survival history, folded in O(1) per
    // generation. Kept here (not derived in the view) so they reflect the whole
    // run without an O(n) Math.min(...spread) — which would also risk a
    // call-stack overflow at the 100k-generation scale. Null until the first
    // census of a run.
    #survivalMin = $state<number | null>(null);
    #survivalCurrent = $state<number | null>(null);
    #survivalMax = $state<number | null>(null);

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

    get kills(): number {
        return this.#kills;
    }

    get survivalHistory(): number[] {
        return this.#survivalHistory;
    }

    get survivalMin(): number | null {
        return this.#survivalMin;
    }

    get survivalCurrent(): number | null {
        return this.#survivalCurrent;
    }

    get survivalMax(): number | null {
        return this.#survivalMax;
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

    // Append a survival rate to the sparkline over the full generation range,
    // folding the running min/current/max. Guards a zero denominator (no
    // population this generation).
    #pushSurvival(survivors: number, denom: number): void {
        const rate = denom > 0 ? survivors / denom : 0;
        this.#survivalHistory = [...this.#survivalHistory, rate];
        this.#survivalCurrent = rate;
        this.#survivalMin =
            this.#survivalMin === null
                ? rate
                : Math.min(this.#survivalMin, rate);
        this.#survivalMax =
            this.#survivalMax === null
                ? rate
                : Math.max(this.#survivalMax, rate);
    }

    // Clear the survival history and its aggregates at the start of a run.
    #resetSurvival(): void {
        this.#survivalHistory = [];
        this.#survivalMin = null;
        this.#survivalCurrent = null;
        this.#survivalMax = null;
    }

    // ── Worker-reply intents ──────────────────────────────────────────────────

    onStepped(e: { step: number }): void {
        this.#step = e.step;
    }

    onCensus(e: {
        gen: number;
        population: number;
        survivors: number;
        kills: number;
    }): void {
        this.#gen = e.gen;
        this.#pop = e.population;
        this.#kills = e.kills;
        this.#pushSurvival(e.survivors, e.population);
        this.#snapReady = true;
    }

    onConfigured(e: ConfigInfo): void {
        this.#gen = 0;
        this.#step = 0;
        this.#resetSurvival();
        this.#pop = e.population;
        this.#kills = 0;
        this.#gridSizeX = e.gridSizeX;
        this.#gridSizeY = e.gridSizeY;
        this.#stepsPerGen = e.stepsPerGen;
        this.#snapReady = false;
    }

    onRewindConfigured(e: ConfigInfo & { gen: number }): void {
        this.#gen = e.gen;
        this.#step = 0;
        this.#pop = e.population;
        this.#kills = 0;
        this.#gridSizeX = e.gridSizeX;
        this.#gridSizeY = e.gridSizeY;
        this.#stepsPerGen = e.stepsPerGen;
        this.#resetSurvival();
    }

    onNextGenerationConfigured(
        e: ConfigInfo & {
            gen: number;
            censusPopulation: number;
            survivors: number;
            kills: number;
        },
    ): void {
        this.#gen = e.gen;
        this.#step = 0;
        this.#pop = e.population;
        this.#kills = e.kills;
        this.#gridSizeX = e.gridSizeX;
        this.#gridSizeY = e.gridSizeY;
        this.#stepsPerGen = e.stepsPerGen;
        this.#pushSurvival(e.survivors, e.censusPopulation);
        this.#snapReady = true;
    }

    // A snapshot import replaces only the population: grid/steps keep their live
    // values, so only the counters and the snapshot gate are refreshed.
    onSnapshotLoaded(e: { gen: number; population: number }): void {
        this.#gen = e.gen;
        this.#step = 0;
        this.#pop = e.population;
        this.#kills = 0;
        this.#snapReady = true;
        this.#resetSurvival();
    }

    // ── User gestures ─────────────────────────────────────────────────────────

    // A manual next-gen / rewind without auto-play resets the step counter so
    // the HUD reads 0 until the worker reports the first step.
    resetStep(): void {
        this.#step = 0;
    }
}
