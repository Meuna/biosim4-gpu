import {
    brainCounts,
    buildBrainModel,
    connectionKind,
    isSelfLoop,
    type BrainModel,
} from "./brainModel";

const sample: BrainModel = buildBrainModel(
    [
        { id: 0, type: "sense", bias: 0.1 },
        { id: 1, type: "sense", bias: -0.2 },
        { id: 2, type: "internal", bias: 0.3 },
        { id: 3, type: "internal", bias: 0.0 },
        { id: 4, type: "action", bias: -0.5 },
    ],
    [
        { source: 0, sink: 2, weight: 1.0 }, // sense → internal
        { source: 1, sink: 4, weight: -2.0 }, // sense → action
        { source: 2, sink: 3, weight: 0.5 }, // internal → internal
        { source: 3, sink: 3, weight: -1.5 }, // internal → internal (self-loop)
        { source: 2, sink: 4, weight: 3.0 }, // internal → action
    ],
);

describe("buildBrainModel", () => {
    it("returns the supplied neurons and connections unchanged", () => {
        expect(sample.neurons).toHaveLength(5);
        expect(sample.connections).toHaveLength(5);
    });
});

describe("brainCounts", () => {
    it("counts neurons by type plus totals", () => {
        const c = brainCounts(sample);
        expect(c).toEqual({
            sense: 2,
            internal: 2,
            action: 1,
            nodes: 5,
            connections: 5,
        });
    });
});

describe("connectionKind", () => {
    it("classifies each connection kind", () => {
        const kinds = sample.connections.map((conn) =>
            connectionKind(sample, conn),
        );
        expect(kinds).toEqual([
            "sense-internal",
            "sense-action",
            "internal-internal",
            "internal-internal",
            "internal-action",
        ]);
    });

    it("returns null for an unknown endpoint", () => {
        expect(
            connectionKind(sample, { source: 99, sink: 2, weight: 1 }),
        ).toBeNull();
    });
});

describe("isSelfLoop", () => {
    it("is true only when source === sink", () => {
        expect(isSelfLoop({ source: 3, sink: 3, weight: 1 })).toBe(true);
        expect(isSelfLoop({ source: 2, sink: 3, weight: 1 })).toBe(false);
    });
});
