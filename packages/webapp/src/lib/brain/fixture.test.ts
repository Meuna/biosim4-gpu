import { brainCounts, isSelfLoop } from "../brainModel";
import {
    brainFixtures,
    brainForAgent,
    maxBrain,
    mediumBrain,
    tinyBrain,
} from "./fixture";

describe("brain fixtures", () => {
    it("tiny is 6 sense × 4 internal × 3 action", () => {
        const c = brainCounts(tinyBrain);
        expect([c.sense, c.internal, c.action]).toEqual([6, 4, 3]);
    });

    it("medium is 10 sense × 20 internal × 5 action", () => {
        const c = brainCounts(mediumBrain);
        expect([c.sense, c.internal, c.action]).toEqual([10, 20, 5]);
    });

    it("max is 20 sense × 128 internal × 15 action with 1000+ connections", () => {
        const c = brainCounts(maxBrain);
        expect([c.sense, c.internal, c.action]).toEqual([20, 128, 15]);
        expect(c.connections).toBeGreaterThanOrEqual(1000);
    });

    it("includes recurrent self-loops", () => {
        expect(maxBrain.connections.some(isSelfLoop)).toBe(true);
    });

    it("is byte-stable across imports (deterministic generation)", () => {
        // Re-importing the module must not change the data; assert a stable
        // snapshot of the first few connections of the tiny fixture.
        expect(tinyBrain.connections.slice(0, 3)).toMatchSnapshot();
    });

    it("exposes all three fixtures by name", () => {
        expect(Object.keys(brainFixtures)).toEqual(["tiny", "medium", "max"]);
    });
});

describe("brainForAgent", () => {
    it("cycles through the three fixtures by agent id", () => {
        expect(brainForAgent(0)).toBe(tinyBrain);
        expect(brainForAgent(1)).toBe(mediumBrain);
        expect(brainForAgent(2)).toBe(maxBrain);
        expect(brainForAgent(3)).toBe(tinyBrain);
    });

    it("is stable for the same id and handles negatives", () => {
        expect(brainForAgent(42)).toBe(brainForAgent(42));
        expect(brainForAgent(-1)).toBe(maxBrain);
    });
});
