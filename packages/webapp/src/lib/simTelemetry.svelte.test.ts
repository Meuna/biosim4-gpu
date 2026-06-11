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
        expect(history(t)).toEqual([]);
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
    it("updates gen/pop, appends survival rate, and arms snapReady", () => {
        const t = create();
        t.onCensus({ gen: 5, population: 200, survivors: 50 });
        expect(t.gen).toBe(5);
        expect(t.pop).toBe(200);
        expect(history(t)).toEqual([0.25]);
        expect(t.snapReady).toBe(true);
    });

    it("guards a zero population with a 0 rate", () => {
        const t = create();
        t.onCensus({ gen: 1, population: 0, survivors: 0 });
        expect(history(t)).toEqual([0]);
    });

    it("caps the sparkline at 12 entries", () => {
        const t = create();
        for (let i = 0; i < 15; i++)
            t.onCensus({ gen: i, population: 100, survivors: i });
        const h = history(t);
        expect(h).toHaveLength(12);
        // Oldest kept entry is the 4th census (survivors=3), newest is the 15th.
        expect(h[0]).toBeCloseTo(0.03);
        expect(h[11]).toBeCloseTo(0.14);
    });
});

describe("onConfigured", () => {
    it("resets counters/history and disarms snapReady", () => {
        const t = create();
        t.onCensus({ gen: 9, population: 100, survivors: 80 }); // dirty first
        t.onConfigured({
            population: 1500,
            gridSizeX: 64,
            gridSizeY: 96,
            stepsPerGen: 250,
        });
        expect(t.gen).toBe(0);
        expect(t.step).toBe(0);
        expect(history(t)).toEqual([]);
        expect(t.pop).toBe(1500);
        expect(t.gridSizeX).toBe(64);
        expect(t.gridSizeY).toBe(96);
        expect(t.stepsPerGen).toBe(250);
        expect(t.snapReady).toBe(false);
    });
});

describe("onRewindConfigured", () => {
    it("sets gen/pop/grid, resets step and history", () => {
        const t = create();
        t.onConfigured(1500, 64, 96, 250);
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
        expect(t.pop).toBe(2000);
        expect(t.gridSizeX).toBe(32);
        expect(t.gridSizeY).toBe(48);
        expect(t.stepsPerGen).toBe(400);
        expect(history(t)).toEqual([]);
    });

    it("leaves snapReady untouched (preserves prior value)", () => {
        const t = create();
        t.onCensus({ gen: 2, population: 100, survivors: 50 }); // arms snapReady
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
        });
        expect(t.gen).toBe(4);
        expect(t.step).toBe(0);
        expect(t.pop).toBe(3000);
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
        });
        expect(history(t)).toEqual([0]);
    });
});

describe("onSnapshotLoaded", () => {
    it("sets gen/pop/grid, arms snapReady, clears history", () => {
        const t = create();
        t.onCensus({ gen: 9, population: 100, survivors: 80 });
        t.onSnapshotLoaded({
            gen: 7,
            population: 1200,
            gridSizeX: 80,
            gridSizeY: 80,
            stepsPerGen: 500,
        });
        expect(t.gen).toBe(7);
        expect(t.step).toBe(0);
        expect(t.pop).toBe(1200);
        expect(t.gridSizeX).toBe(80);
        expect(t.gridSizeY).toBe(80);
        expect(t.stepsPerGen).toBe(500);
        expect(t.snapReady).toBe(true);
        expect(history(t)).toEqual([]);
    });
});

describe("resetStep", () => {
    it("zeroes only the step counter", () => {
        const t = create();
        t.onStepped({ step: 88 });
        t.onCensus({ gen: 3, population: 100, survivors: 50 });
        t.resetStep();
        expect(t.step).toBe(0);
        expect(t.gen).toBe(3);
        expect(t.snapReady).toBe(true);
    });
});
