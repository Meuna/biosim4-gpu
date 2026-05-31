// brain.ts — decode an agent's neural network into a renderable graph.
//
// Mirrors the bit-layout and constants from the C core
// (packages/core/include/biosim/core/gene.h and io_defs.h). The connection
// genes are unpacked here, in the worker, rather than in WASM — keeping the
// wasm/worker interface to four plain pointer getters at the cost of mirroring
// four trivial bit-shift macros (the deliberate trade-off chosen for gh-74).

// Node-type discriminants — mirror BIOSIM_GENE_NEURON / BIOSIM_GENE_IO.
export const GENE_NEURON = 0;
export const GENE_IO = 1;
export type GeneType = typeof GENE_NEURON | typeof GENE_IO;

// Mirror BIOSIM_GENE_WEIGHT_SCALE: int16 gene weights ÷ this yield ~±4.0.
export const WEIGHT_SCALE = 8192;

/** One decoded connection. For an IO source, `srcNum` is a `biosim_sensor_t`
 *  ordinal; for an IO sink, `sinkNum` is a `biosim_action_t` ordinal. Neuron
 *  numbers are compact indices in `[0, neuronCount)`. */
export interface BrainConn {
    srcType: GeneType;
    srcNum: number;
    sinkType: GeneType;
    sinkNum: number;
    weight: number;
}

/** Decode a packed connection gene + raw int16 weight. Mirrors the
 *  BIOSIM_GENE_* macros: `[srcType:1][srcNum:7][sinkType:1][sinkNum:7]`. */
export function unpackConn(packed: number, rawWgt: number): BrainConn {
    return {
        srcType: ((packed >> 15) & 1) as GeneType,
        srcNum: (packed >> 8) & 0x7f,
        sinkType: ((packed >> 7) & 1) as GeneType,
        sinkNum: packed & 0x7f,
        weight: rawWgt / WEIGHT_SCALE,
    };
}

/** Short labels for sensors, indexed by `biosim_sensor_t` (io_defs.h). */
export const SENSOR_LABELS: readonly string[] = [
    "X", // LOC_X
    "Y", // LOC_Y
    "bdX", // BOUNDARY_DIST_X
    "bdY", // BOUNDARY_DIST_Y
    "bd", // BOUNDARY_DIST
    "lmX", // LAST_MOVE_DIR_X
    "lmY", // LAST_MOVE_DIR_Y
    "osc", // OSC1
    "age", // AGE
    "rnd", // RANDOM
    "pop", // POPULATION
    "ppF", // POPULATION_FWD
    "ppLR", // POPULATION_LR
    "brF", // BARRIER_FWD
    "brLR", // BARRIER_LR
    "lpP", // LONGPROBE_POP_FWD
    "lpB", // LONGPROBE_BAR_FWD
    "sg0", // SIGNAL0
    "sg0F", // SIGNAL0_FWD
    "sg0LR", // SIGNAL0_LR
    "gen", // GENETIC_SIM_FWD
];

/** Short node labels for actions, indexed by `biosim_action_t` (io_defs.h).
 *  A `"glyph:<name>"` entry is rendered as that icon; any other entry is text. */
export const ACTION_LABELS: readonly string[] = [
    "Rsp", // SET_RESPONSIVENESS
    "Osc", // SET_OSCILLATOR_PERIOD
    "Lpd", // SET_LONGPROBE_DIST
    "mX", // MOVE_X
    "mY", // MOVE_Y
    "↑", // MOVE_FORWARD
    "↓", // MOVE_REVERSE
    "↰", // MOVE_LEFT
    "↱", // MOVE_RIGHT
    "rnd", // MOVE_RANDOM
    "E", // MOVE_EAST
    "W", // MOVE_WEST
    "N", // MOVE_NORTH
    "S", // MOVE_SOUTH
    "sg0", // EMIT_SIGNAL0
    "glyph:skull", // KILL_FORWARD
];

/** If `label` is a `"glyph:<name>"` token, return `<name>`; otherwise `null`
 *  (the label is plain text). The renderer resolves the name to an icon. */
export function glyphName(label: string): string | null {
    return label.startsWith("glyph:") ? label.slice("glyph:".length) : null;
}

/** Full sensor names for the node info card, indexed by `biosim_sensor_t`. */
export const SENSOR_NAMES: readonly string[] = [
    "Location X",
    "Location Y",
    "Boundary distance X",
    "Boundary distance Y",
    "Boundary distance",
    "Last move dir X",
    "Last move dir Y",
    "Oscillator",
    "Age",
    "Random",
    "Population density",
    "Population forward",
    "Population left/right",
    "Barrier forward",
    "Barrier left/right",
    "Long-probe population fwd",
    "Long-probe barrier fwd",
    "Signal density",
    "Signal forward",
    "Signal left/right",
    "Genetic similarity fwd",
];

/** Full action names for the node info card, indexed by `biosim_action_t`. */
export const ACTION_NAMES: readonly string[] = [
    "Set responsiveness",
    "Set oscillator period",
    "Set long-probe distance",
    "Move X",
    "Move Y",
    "Move forward",
    "Move reverse",
    "Move left",
    "Move right",
    "Move random",
    "Move east",
    "Move west",
    "Move north",
    "Move south",
    "Emit signal",
    "Kill forward",
];

export interface BrainSynthesis {
    senses: number;
    internal: number;
    actions: number;
    connections: number;
}

/** Count the distinct senses/actions actually wired plus the internal-neuron
 *  count, for the "6 senses → 10 internal → 4 actions · 28 connections" line. */
export function brainSynthesis(
    conns: readonly BrainConn[],
    neuronCount: number,
): BrainSynthesis {
    const senses = new Set<number>();
    const actions = new Set<number>();
    for (const c of conns) {
        if (c.srcType === GENE_IO) senses.add(c.srcNum);
        if (c.sinkType === GENE_IO) actions.add(c.sinkNum);
    }
    return {
        senses: senses.size,
        internal: neuronCount,
        actions: actions.size,
        connections: conns.length,
    };
}
