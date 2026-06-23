import { describe, it, expect } from "vitest";
import { SimTelemetry } from "./simTelemetry.svelte";

function create() {
    return new SimTelemetry();
}

function history(t: SimTelemetry): number[] {
    return $state.snapshot(t.survivalHistory) as number[];
}

describe("initial state", () => {
    it("matches the default presentation values", () => {
        const t = create();
        expect(t.gen).toBe(0);
        expect(t.step).toBe(0);
        expect(t.pop).toBe(3000);
        expect(t.kills).toBe(0);
        expect(history(t)).toEqual([]);
        expect(t.survivalMin).toBeNull();
        expect(t.survivalCurrent).toBeNull();
        expect(t.survivalMax).toBeNull();
        expect(t.stepsPerGen).toBe(300);
        expect(t.gridSizeX).toBe(128);
        expect(t.gridSizeY).toBe(128);
        expect(t.snapReady).toBe(false);
    });
});

describe("onStepped", () => {
    it("updates only the step counter", () => {
        const t = create();
        t.onStepped({ step: 42 });
        expect(t.step).toBe(42);
        expect(t.snapReady).toBe(false);
    });
});

describe("onCensus", () => {
    it("updates gen/pop/kills, appends survival rate, and arms snapReady", () => {
        const t = create();
        t.onCensus({ gen: 5, population: 200, survivors: 50, kills: 12 });
        expect(t.gen).toBe(5);
        expect(t.pop).toBe(200);
        expect(t.kills).toBe(12);
        expect(history(t)).toEqual([0.25]);
        expect(t.snapReady).toBe(true);
    });

    it("guards a zero population with a 0 rate", () => {
        const t = create();
        t.onCensus({ gen: 1, population: 0, survivors: 0, kills: 0 });
        expect(history(t)).toEqual([0]);
    });

    it("keeps the full history without a sliding-window cap", () => {
        const t = create();
        for (let i = 0; i < 15; i++)
            t.onCensus({ gen: i, population: 100, survivors: i, kills: 0 });
        const h = history(t);
        expect(h).toHaveLength(15);
        expect(h[0]).toBeCloseTo(0);
        expect(h[14]).toBeCloseTo(0.14);
    });

    it("folds running min/current/max over the full history", () => {
        const t = create();
        t.onCensus({ gen: 0, population: 100, survivors: 50, kills: 0 }); // 0.5
        t.onCensus({ gen: 1, population: 100, survivors: 20, kills: 0 }); // 0.2
        t.onCensus({ gen: 2, population: 100, survivors: 80, kills: 0 }); // 0.8
        t.onCensus({ gen: 3, population: 100, survivors: 40, kills: 0 }); // 0.4
        expect(t.survivalMin).toBeCloseTo(0.2);
        expect(t.survivalMax).toBeCloseTo(0.8);
        expect(t.survivalCurrent).toBeCloseTo(0.4);
    });
});

describe("onConfigured", () => {
    it("resets counters/kills/history and disarms snapReady", () => {
        const t = create();
        t.onCensus({ gen: 9, population: 100, survivors: 80, kills: 7 }); // dirty first
        t.onConfigured({
            population: 1500,
            gridSizeX: 64,
            gridSizeY: 96,
            stepsPerGen: 250,
        });
        expect(t.gen).toBe(0);
        expect(t.step).toBe(0);
        expect(t.kills).toBe(0);
        expect(history(t)).toEqual([]);
        expect(t.survivalMin).toBeNull();
        expect(t.survivalCurrent).toBeNull();
        expect(t.survivalMax).toBeNull();
        expect(t.pop).toBe(1500);
        expect(t.gridSizeX).toBe(64);
        expect(t.gridSizeY).toBe(96);
        expect(t.stepsPerGen).toBe(250);
        expect(t.snapReady).toBe(false);
    });
});

describe("onRewindConfigured", () => {
    it("sets gen/pop/grid, resets step/kills and history", () => {
        const t = create();
        t.onCensus({ gen: 9, population: 100, survivors: 80, kills: 7 }); // dirty kills first
        t.onStepped({ step: 120 });
        t.onRewindConfigured({
            gen: 3,
            population: 2000,
            gridSizeX: 32,
            gridSizeY: 48,
            stepsPerGen: 400,
        });
        expect(t.gen).toBe(3);
        expect(t.step).toBe(0);
        expect(t.kills).toBe(0);
        expect(t.pop).toBe(2000);
        expect(t.gridSizeX).toBe(32);
        expect(t.gridSizeY).toBe(48);
        expect(t.stepsPerGen).toBe(400);
        expect(history(t)).toEqual([]);
        expect(t.survivalMin).toBeNull();
        expect(t.survivalCurrent).toBeNull();
        expect(t.survivalMax).toBeNull();
    });

    it("leaves snapReady untouched (preserves prior value)", () => {
        const t = create();
        t.onCensus({ gen: 2, population: 100, survivors: 50, kills: 0 }); // arms snapReady
        t.onRewindConfigured({
            gen: 1,
            population: 100,
            gridSizeX: 64,
            gridSizeY: 64,
            stepsPerGen: 300,
        });
        expect(t.snapReady).toBe(true);
    });
});

describe("onNextGenerationConfigured", () => {
    it("uses the census population as the survival denominator", () => {
        const t = create();
        t.onNextGenerationConfigured({
            gen: 4,
            population: 3000,
            gridSizeX: 128,
            gridSizeY: 128,
            stepsPerGen: 300,
            censusPopulation: 160,
            survivors: 40,
            kills: 9,
        });
        expect(t.gen).toBe(4);
        expect(t.step).toBe(0);
        expect(t.pop).toBe(3000);
        expect(t.kills).toBe(9);
        expect(history(t)).toEqual([0.25]);
        expect(t.snapReady).toBe(true);
    });

    it("guards a zero census population with a 0 rate", () => {
        const t = create();
        t.onNextGenerationConfigured({
            gen: 1,
            population: 3000,
            gridSizeX: 128,
            gridSizeY: 128,
            stepsPerGen: 300,
            censusPopulation: 0,
            survivors: 0,
            kills: 0,
        });
        expect(history(t)).toEqual([0]);
    });
});

describe("onSnapshotLoaded", () => {
    it("sets gen/pop, resets kills, arms snapReady, clears history, keeps grid/steps", () => {
        const t = create();
        t.onConfigured({
            population: 1000,
            gridSizeX: 80,
            gridSizeY: 80,
            stepsPerGen: 500,
        });
        t.onCensus({ gen: 9, population: 100, survivors: 80, kills: 7 });
        t.onSnapshotLoaded({ gen: 7, population: 1200 });
        expect(t.gen).toBe(7);
        expect(t.step).toBe(0);
        expect(t.pop).toBe(1200);
        expect(t.kills).toBe(0);
        // Import affects only the population — grid/steps keep their live values.
        expect(t.gridSizeX).toBe(80);
        expect(t.gridSizeY).toBe(80);
        expect(t.stepsPerGen).toBe(500);
        expect(t.snapReady).toBe(true);
        expect(history(t)).toEqual([]);
        expect(t.survivalMin).toBeNull();
        expect(t.survivalCurrent).toBeNull();
        expect(t.survivalMax).toBeNull();
    });
});

describe("resetStep", () => {
    it("zeroes only the step counter", () => {
        const t = create();
        t.onStepped({ step: 88 });
        t.onCensus({ gen: 3, population: 100, survivors: 50, kills: 0 });
        t.resetStep();
        expect(t.step).toBe(0);
        expect(t.gen).toBe(3);
        expect(t.snapReady).toBe(true);
    });
});
