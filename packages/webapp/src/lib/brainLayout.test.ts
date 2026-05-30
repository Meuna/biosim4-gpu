import { brainCounts } from "./brainModel";
import { layoutBrain, type Point } from "./brainLayout";
import { maxBrain, tinyBrain } from "./brain/fixture";

/** Compare two position maps as sorted [id, x, y] tuples (order-independent). */
function positionTuples(
    positions: Map<number, Point>,
): [number, number, number][] {
    return [...positions.entries()]
        .map(([id, p]): [number, number, number] => [id, p.x, p.y])
        .sort((a, b) => a[0] - b[0]);
}

describe("layoutBrain — determinism", () => {
    it("produces identical positions across runs for the max fixture", () => {
        const a = layoutBrain(maxBrain);
        const b = layoutBrain(maxBrain);
        expect(positionTuples(a.positions)).toEqual(
            positionTuples(b.positions),
        );
    });

    it("produces identical edge routing across runs", () => {
        const a = layoutBrain(maxBrain);
        const b = layoutBrain(maxBrain);
        expect(a.edges).toEqual(b.edges);
    });
});

describe("layoutBrain — structure", () => {
    it("positions every neuron", () => {
        const layout = layoutBrain(maxBrain);
        expect(layout.positions.size).toBe(brainCounts(maxBrain).nodes);
    });

    it("pins senses to the left and actions to the right", () => {
        const layout = layoutBrain(maxBrain);
        const senseXs = maxBrain.neurons
            .filter((n) => n.type === "sense")
            .map((n) => layout.positions.get(n.id)!.x);
        const actionXs = maxBrain.neurons
            .filter((n) => n.type === "action")
            .map((n) => layout.positions.get(n.id)!.x);
        const maxSenseX = Math.max(...senseXs);
        const minActionX = Math.min(...actionXs);
        expect(maxSenseX).toBeLessThan(minActionX);
    });

    it("keeps internal neurons inside the central band", () => {
        const layout = layoutBrain(maxBrain);
        for (const n of maxBrain.neurons.filter((m) => m.type === "internal")) {
            const p = layout.positions.get(n.id)!;
            expect(p.x).toBeGreaterThanOrEqual(0.28);
            expect(p.x).toBeLessThanOrEqual(0.72);
        }
    });

    it("classifies self-loops and recurrent edges", () => {
        const layout = layoutBrain(maxBrain);
        expect(layout.edges.some((e) => e.routing === "self")).toBe(true);
        expect(layout.edges.some((e) => e.routing === "recurrent")).toBe(true);
        expect(layout.edges.some((e) => e.routing === "forward")).toBe(true);
    });

    it("handles a tiny brain without throwing", () => {
        const layout = layoutBrain(tinyBrain);
        expect(layout.positions.size).toBe(brainCounts(tinyBrain).nodes);
    });
});
