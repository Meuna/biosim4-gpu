import { stepDelay, createFpsWindow } from "./playbackRate";

describe("stepDelay", () => {
    it("returns 0 for unlimited (targetFps=0)", () => {
        expect(stepDelay(0, 0)).toBe(0);
        expect(stepDelay(0, 5)).toBe(0);
    });

    it("returns 0 when step is slower than target", () => {
        expect(stepDelay(25, 50)).toBe(0);
        expect(stepDelay(1, 1001)).toBe(0);
    });

    it("returns correct delay for 1fps", () => {
        expect(stepDelay(1, 100)).toBeCloseTo(900, 5);
    });

    it("returns correct delay for 25fps", () => {
        expect(stepDelay(25, 10)).toBeCloseTo(30, 5);
    });

    it("returns correct delay for 50fps", () => {
        expect(stepDelay(50, 5)).toBeCloseTo(15, 5);
    });

    it("never returns negative", () => {
        expect(stepDelay(50, 1000)).toBe(0);
    });
});

describe("createFpsWindow", () => {
    it("returns null on the opener tick", () => {
        const w = createFpsWindow();
        expect(w.tick(0)).toBeNull();
    });

    it("returns null while inside the window", () => {
        const w = createFpsWindow();
        w.tick(0);
        expect(w.tick(500)).toBeNull();
        expect(w.tick(900)).toBeNull();
    });

    it("returns ~1 fps for one step over 1000ms", () => {
        const w = createFpsWindow();
        w.tick(0);
        const fps = w.tick(1000);
        expect(fps).toBeCloseTo(1, 5);
    });

    it("returns ~50 fps for 50 steps at 20ms intervals", () => {
        const w = createFpsWindow();
        w.tick(0);
        let fps: number | null = null;
        for (let i = 1; i <= 50; i++) {
            fps = w.tick(i * 20);
        }
        expect(fps).toBeCloseTo(50, 0);
    });

    it("resets and starts a new window after reporting", () => {
        const w = createFpsWindow();
        w.tick(0);
        w.tick(1000); // closes window; internally start=1000, steps=0
        // Next tick is the first step of the new window — window not yet closed
        expect(w.tick(1001)).toBeNull();
    });

    it("reports correctly across two consecutive windows", () => {
        const w = createFpsWindow();
        w.tick(0);
        const fps1 = w.tick(1000); // closes first window: 1 step / 1s = 1fps
        expect(fps1).toBeCloseTo(1, 5);
        // Closing tick reset start=1000, steps=0 — next tick opens new window
        const fps2 = w.tick(2000); // 1 step / 1s = 1fps
        expect(fps2).toBeCloseTo(1, 5);
    });
});
