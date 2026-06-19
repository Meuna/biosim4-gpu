import {
    beatEnvelope,
    easeInOut,
    gridPosition,
    kinematicPosition,
    lerpVec2,
    sphereSculpture,
    type KinematicCtx,
} from "./kinematic";

const CANVAS_W = 800;
const CANVAS_H = 600;
const MARGIN = 0.12;

function ctx(overrides: Partial<KinematicCtx> = {}): KinematicCtx {
    return {
        t: 0,
        canvasW: CANVAS_W,
        canvasH: CANVAS_H,
        beat: 0,
        pointer: null,
        ...overrides,
    };
}

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

describe("kinematicPosition (wave surface)", () => {
    const POP = 100;

    it("is deterministic per index for identical inputs", () => {
        const a = kinematicPosition(42, POP, ctx({ t: 1.5 }));
        const b = kinematicPosition(42, POP, ctx({ t: 1.5 }));
        expect(a).toEqual(b);
    });

    it("keeps dots within the margin-extended box (no edge seam)", () => {
        // Sample many agents across several times; displacement adds at most
        // (AMP + a small slack) beyond the barycenter span.
        const slackX = CANVAS_W * (MARGIN + 0.05);
        const slackY = CANVAS_H * (MARGIN + 0.06);
        for (const t of [0, 0.7, 3.3, 10]) {
            for (let i = 0; i < POP; i++) {
                const { x, y } = kinematicPosition(i, POP, ctx({ t }));
                expect(x).toBeGreaterThanOrEqual(-slackX);
                expect(x).toBeLessThanOrEqual(CANVAS_W + slackX);
                expect(y).toBeGreaterThanOrEqual(-slackY);
                expect(y).toBeLessThanOrEqual(CANVAS_H + slackY);
            }
        }
    });

    it("keeps the radius within the un-beaten wave swing range", () => {
        // R_BASE = 1.8, R_SWING = 0.8 → [1.0, 2.6] when beat = 0.
        for (const t of [0, 1.1, 5.5]) {
            for (let i = 0; i < POP; i++) {
                const { r } = kinematicPosition(i, POP, ctx({ t }));
                expect(r).toBeGreaterThanOrEqual(1.0 - 1e-6);
                expect(r).toBeLessThanOrEqual(2.6 + 1e-6);
            }
        }
    });

    it("spreads barycenters across both halves of the viewport", () => {
        // The matrix must cover the whole surface, not cluster at the centre.
        let left = false;
        let right = false;
        let top = false;
        let bottom = false;
        for (let i = 0; i < POP; i++) {
            const { x, y } = kinematicPosition(i, POP, ctx());
            if (x < CANVAS_W * 0.25) left = true;
            if (x > CANVAS_W * 0.75) right = true;
            if (y < CANVAS_H * 0.25) top = true;
            if (y > CANVAS_H * 0.75) bottom = true;
        }
        expect(left && right && top && bottom).toBe(true);
    });

    it("a full-viewport beat enlarges dots (vs the neutral case)", () => {
        const neutral = kinematicPosition(10, POP, ctx({ t: 0.3 }));
        const beaten = kinematicPosition(10, POP, ctx({ t: 0.3, beat: 1 }));
        expect(beaten.r).toBeGreaterThan(neutral.r);
    });

    it("keeps opacity within [0, 1] across the wave field", () => {
        for (const t of [0, 1.1, 5.5]) {
            for (let i = 0; i < POP; i++) {
                const { opacity } = kinematicPosition(i, POP, ctx({ t }));
                expect(opacity).toBeGreaterThanOrEqual(0);
                expect(opacity).toBeLessThanOrEqual(1);
            }
        }
    });

    it("a full-viewport beat reaches full opacity", () => {
        const beaten = kinematicPosition(10, POP, ctx({ t: 0.3, beat: 1 }));
        expect(beaten.opacity).toBeCloseTo(1);
    });
});

describe("beatEnvelope", () => {
    const ATTACK_MS = 90;
    const DECAY_MS = 700;

    it("is 0 at elapsed 0 and below", () => {
        expect(beatEnvelope(0)).toBe(0);
        expect(beatEnvelope(-50)).toBe(0);
    });

    it("peaks at 1 at the end of the attack", () => {
        expect(beatEnvelope(ATTACK_MS)).toBeCloseTo(1);
    });

    it("returns 0 at and after the full duration", () => {
        expect(beatEnvelope(ATTACK_MS + DECAY_MS)).toBe(0);
        expect(beatEnvelope(ATTACK_MS + DECAY_MS + 100)).toBe(0);
    });

    it("stays within [0, 1] and decays monotonically after the peak", () => {
        let prev = beatEnvelope(ATTACK_MS);
        for (let e = ATTACK_MS + 20; e <= ATTACK_MS + DECAY_MS; e += 20) {
            const v = beatEnvelope(e);
            expect(v).toBeGreaterThanOrEqual(0);
            expect(v).toBeLessThanOrEqual(1);
            expect(v).toBeLessThanOrEqual(prev + 1e-9);
            prev = v;
        }
    });
});

describe("sphereSculpture (sample)", () => {
    const POP = 120;

    it("is deterministic per index for identical inputs", () => {
        const a = sphereSculpture(7, POP, ctx({ t: 2.0 }));
        const b = sphereSculpture(7, POP, ctx({ t: 2.0 }));
        expect(a).toEqual(b);
    });

    it("projects points onto the canvas with a positive radius", () => {
        for (let i = 0; i < POP; i++) {
            const { x, y, r } = sphereSculpture(i, POP, ctx({ t: 1 }));
            expect(Number.isFinite(x)).toBe(true);
            expect(Number.isFinite(y)).toBe(true);
            expect(x).toBeGreaterThan(0);
            expect(x).toBeLessThan(CANVAS_W);
            expect(y).toBeGreaterThan(0);
            expect(y).toBeLessThan(CANVAS_H);
            expect(r).toBeGreaterThan(0);
        }
    });

    it("fades far-side points: opacity stays in [0, 1] and varies with depth", () => {
        let min = Infinity;
        let max = -Infinity;
        for (let i = 0; i < POP; i++) {
            const { opacity } = sphereSculpture(i, POP, ctx({ t: 1 }));
            expect(opacity).toBeGreaterThanOrEqual(0);
            expect(opacity).toBeLessThanOrEqual(1);
            min = Math.min(min, opacity);
            max = Math.max(max, opacity);
        }
        // Depth cue is real: the front and back faces differ.
        expect(max - min).toBeGreaterThan(0.2);
    });
});

describe("gridPosition", () => {
    it("maps cell (0,0) to the top-left cell centre for a square grid", () => {
        // 400×400 canvas region, 128×128 cells
        const cellPx = 400 / 128;
        const { x, y } = gridPosition(0, 0, {
            gridX: 100,
            gridY: 100,
            gridW: 400,
            gridH: 400,
            gridCellsX: 128,
            gridCellsY: 128,
        });
        expect(x).toBeCloseTo(100 + cellPx * 0.5);
        expect(y).toBeCloseTo(100 + cellPx * 0.5);
    });

    it("uses the correct radius formula for a square grid", () => {
        const cellPx = 400 / 128;
        const expected = Math.max(1.5, cellPx * 0.4);
        const { r } = gridPosition(0, 0, {
            gridX: 100,
            gridY: 100,
            gridW: 400,
            gridH: 400,
            gridCellsX: 128,
            gridCellsY: 128,
        });
        expect(r).toBeCloseTo(expected);
    });

    it("maps a rectangular grid correctly (wide: 256×128 cells in 400×200 px)", () => {
        // cellPxW = 400/256 = 1.5625, cellPxH = 200/128 = 1.5625 (coincidence)
        const cellPxW = 400 / 256;
        const cellPxH = 200 / 128;
        const { x, y } = gridPosition(0, 0, {
            gridX: 50,
            gridY: 60,
            gridW: 400,
            gridH: 200,
            gridCellsX: 256,
            gridCellsY: 128,
        });
        expect(x).toBeCloseTo(50 + cellPxW * 0.5);
        expect(y).toBeCloseTo(60 + cellPxH * 0.5);
    });

    it("uses the smaller cell dimension for radius in a non-square grid", () => {
        // Use a clearly asymmetric case: W=100, H=400, cells 100×100
        // cellPxW=1, cellPxH=4 → min=1, r=max(1.5, 0.4)=1.5
        const { r } = gridPosition(0, 0, {
            gridX: 0,
            gridY: 0,
            gridW: 100,
            gridH: 400,
            gridCellsX: 100,
            gridCellsY: 100,
        });
        expect(r).toBeCloseTo(Math.max(1.5, 1 * 0.4));
    });
});
