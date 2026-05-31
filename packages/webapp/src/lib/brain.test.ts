import {
    GENE_NEURON,
    GENE_IO,
    WEIGHT_SCALE,
    unpackConn,
    brainSynthesis,
    glyphName,
    SENSOR_LABELS,
    ACTION_LABELS,
    SENSOR_NAMES,
    ACTION_NAMES,
} from "./brain";

// Mirror of BIOSIM_GENE_PACK from gene.h, for building test fixtures.
function pack(
    srcType: number,
    srcNum: number,
    sinkType: number,
    sinkNum: number,
): number {
    return (
        ((srcType & 1) << 15) |
        ((srcNum & 0x7f) << 8) |
        ((sinkType & 1) << 7) |
        (sinkNum & 0x7f)
    );
}

describe("unpackConn", () => {
    it("decodes a sensor → action connection", () => {
        const c = unpackConn(pack(GENE_IO, 5, GENE_IO, 8), WEIGHT_SCALE);
        expect(c.srcType).toBe(GENE_IO);
        expect(c.srcNum).toBe(5);
        expect(c.sinkType).toBe(GENE_IO);
        expect(c.sinkNum).toBe(8);
        expect(c.weight).toBeCloseTo(1.0);
    });

    it("decodes a neuron → neuron connection (incl. self-loop)", () => {
        const c = unpackConn(pack(GENE_NEURON, 3, GENE_NEURON, 3), 0);
        expect(c.srcType).toBe(GENE_NEURON);
        expect(c.sinkType).toBe(GENE_NEURON);
        expect(c.srcNum).toBe(3);
        expect(c.sinkNum).toBe(3);
        expect(c.weight).toBe(0);
    });

    it("preserves the sign of negative weights", () => {
        const c = unpackConn(pack(GENE_NEURON, 0, GENE_IO, 1), -WEIGHT_SCALE);
        expect(c.weight).toBeCloseTo(-1.0);
    });

    it("masks src/sink numbers to 7 bits", () => {
        // All bits set: srcType=1, srcNum=0x7f, sinkType=1, sinkNum=0x7f.
        const c = unpackConn(0xffff, 0);
        expect(c.srcNum).toBe(0x7f);
        expect(c.sinkNum).toBe(0x7f);
    });
});

describe("label tables", () => {
    it("has matching short labels and full names per ordinal", () => {
        expect(SENSOR_LABELS.length).toBe(21);
        expect(ACTION_LABELS.length).toBe(16);
        expect(SENSOR_NAMES.length).toBe(21);
        expect(ACTION_NAMES.length).toBe(16);
    });

    it("maps known ordinals to issue-specified labels", () => {
        expect(SENSOR_LABELS[0]).toBe("X"); // LOC_X (text)
        expect(SENSOR_LABELS[5]).toBe("lmX"); // LAST_MOVE_DIR_X (text)
        expect(ACTION_LABELS[8]).toBe("glyph:corner-up-right"); // MOVE_RIGHT
        expect(SENSOR_NAMES[0]).toBe("Location X");
        expect(ACTION_NAMES[15]).toBe("Kill forward");
    });

    it("distinguishes glyph tokens from plain-text labels", () => {
        expect(ACTION_LABELS[15]).toBe("glyph:skull"); // KILL_FORWARD
        expect(glyphName(ACTION_LABELS[15])).toBe("skull");
        expect(glyphName(SENSOR_LABELS[0])).toBeNull(); // "X" is plain text
    });
});

describe("brainSynthesis", () => {
    it("counts distinct senses, internal neurons, actions and connections", () => {
        const conns = [
            unpackConn(pack(GENE_IO, 0, GENE_NEURON, 0), WEIGHT_SCALE),
            unpackConn(pack(GENE_IO, 0, GENE_NEURON, 1), WEIGHT_SCALE), // dup sense 0
            unpackConn(pack(GENE_IO, 2, GENE_NEURON, 0), WEIGHT_SCALE),
            unpackConn(pack(GENE_NEURON, 0, GENE_IO, 5), WEIGHT_SCALE),
            unpackConn(pack(GENE_NEURON, 1, GENE_IO, 5), WEIGHT_SCALE), // dup action 5
        ];
        const s = brainSynthesis(conns, 4);
        expect(s.senses).toBe(2); // senses 0, 2
        expect(s.actions).toBe(1); // action 5
        expect(s.internal).toBe(4); // neuronCount
        expect(s.connections).toBe(5);
    });

    it("returns zeros for an empty brain", () => {
        const s = brainSynthesis([], 0);
        expect(s).toEqual({
            senses: 0,
            internal: 0,
            actions: 0,
            connections: 0,
        });
    });
});
