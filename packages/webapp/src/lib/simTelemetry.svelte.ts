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

    onStatus(step: number): void {
        this.#step = step;
    }

    onCensus(gen: number, population: number, survivors: number): void {
        this.#gen = gen;
        this.#pop = population;
        this.#pushSurvival(survivors, population);
        this.#snapReady = true;
    }

    onConfigured(
        population: number,
        gridSizeX: number,
        gridSizeY: number,
        stepsPerGen: number,
    ): void {
        this.#gen = 0;
        this.#step = 0;
        this.#survivalHistory = [];
        this.#pop = population;
        this.#gridSizeX = gridSizeX;
        this.#gridSizeY = gridSizeY;
        this.#stepsPerGen = stepsPerGen;
        this.#snapReady = false;
    }

    onRewindConfigured(
        gen: number,
        population: number,
        gridSizeX: number,
        gridSizeY: number,
        stepsPerGen: number,
    ): void {
        this.#gen = gen;
        this.#step = 0;
        this.#pop = population;
        this.#gridSizeX = gridSizeX;
        this.#gridSizeY = gridSizeY;
        this.#stepsPerGen = stepsPerGen;
        this.#survivalHistory = [];
    }

    onNextGenerationConfigured(
        gen: number,
        population: number,
        gridSizeX: number,
        gridSizeY: number,
        stepsPerGen: number,
        censusPopulation: number,
        survivors: number,
    ): void {
        this.#gen = gen;
        this.#step = 0;
        this.#pop = population;
        this.#gridSizeX = gridSizeX;
        this.#gridSizeY = gridSizeY;
        this.#stepsPerGen = stepsPerGen;
        this.#pushSurvival(survivors, censusPopulation);
        this.#snapReady = true;
    }

    onSnapshotLoaded(
        gen: number,
        population: number,
        gridSizeX: number,
        gridSizeY: number,
        stepsPerGen: number,
    ): void {
        this.#gen = gen;
        this.#step = 0;
        this.#pop = population;
        this.#gridSizeX = gridSizeX;
        this.#gridSizeY = gridSizeY;
        this.#stepsPerGen = stepsPerGen;
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
