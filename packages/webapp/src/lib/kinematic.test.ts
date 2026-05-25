import {
    easeInOut,
    gridPosition,
    kinematicPosition,
    lerpVec2,
} from "./kinematic";

describe("easeInOut", () => {
    it("returns 0 at t=0 and 1 at t=1", () => {
        expect(easeInOut(0)).toBe(0);
        expect(easeInOut(1)).toBe(1);
    });

    it("returns 0.5 at t=0.5 (symmetry point)", () => {
        expect(easeInOut(0.5)).toBeCloseTo(0.5);
    });

    it("is monotonically increasing over [0, 1]", () => {
        const samples = [0, 0.1, 0.25, 0.5, 0.75, 0.9, 1];
        for (let k = 1; k < samples.length; k++) {
            expect(easeInOut(samples[k])).toBeGreaterThan(
                easeInOut(samples[k - 1]),
            );
        }
    });
});

describe("lerpVec2", () => {
    it("interpolates to the midpoint at t=0.5", () => {
        const result = lerpVec2({ x: 0, y: 0 }, { x: 10, y: 20 }, 0.5);
        expect(result.x).toBeCloseTo(5);
        expect(result.y).toBeCloseTo(10);
    });

    it("returns the start point at t=0", () => {
        const result = lerpVec2({ x: 3, y: 7 }, { x: 100, y: 200 }, 0);
        expect(result.x).toBeCloseTo(3);
        expect(result.y).toBeCloseTo(7);
    });

    it("returns the end point at t=1", () => {
        const result = lerpVec2({ x: 3, y: 7 }, { x: 100, y: 200 }, 1);
        expect(result.x).toBeCloseTo(100);
        expect(result.y).toBeCloseTo(200);
    });
});

describe("kinematicPosition", () => {
    it("returns a radius in the expected [1, 2] range at t=0", () => {
        const { r } = kinematicPosition(0, 100, 400, 300, 0, 800, 600);
        expect(r).toBeGreaterThanOrEqual(1);
        expect(r).toBeLessThanOrEqual(2);
    });

    it("places agent 0 at the rightmost orbit point when t=0", () => {
        // agent 0 has theta=0, t=0 → x = cx + rX, y = cy + 0
        const canvasW = 800;
        const canvasH = 600;
        const cx = canvasW / 2;
        const cy = canvasH / 2;
        const { x, y } = kinematicPosition(0, 100, cx, cy, 0, canvasW, canvasH);
        expect(x).toBeCloseTo(cx + canvasW * 0.7);
        expect(y).toBeCloseTo(cy);
    });

    it("produces different positions for different agents at the same time", () => {
        const a = kinematicPosition(0, 100, 400, 300, 1, 800, 600);
        const b = kinematicPosition(50, 100, 400, 300, 1, 800, 600);
        expect(a.x).not.toBeCloseTo(b.x, 1);
    });
});

describe("gridPosition", () => {
    it("maps cell (0,0) to the top-left cell centre", () => {
        const cellPx = 400 / 128;
        const { x, y } = gridPosition(0, 0, 100, 100, 400, 128);
        expect(x).toBeCloseTo(100 + cellPx * 0.5);
        expect(y).toBeCloseTo(100 + cellPx * 0.5);
    });

    it("uses the correct radius formula", () => {
        const cellPx = 400 / 128;
        const expected = Math.max(1.5, cellPx * 0.4);
        const { r } = gridPosition(0, 0, 100, 100, 400, 128);
        expect(r).toBeCloseTo(expected);
    });
});
