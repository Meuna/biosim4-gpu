// Deterministic BrainModel fixtures spanning the full topology range. Used to
// develop and verify the Signature (diagram + fingerprint modes) and the
// Explorer (incl. the 20×128×15 maximum) before the worker exposes real
// per-neuron / per-connection data. No DOM; pure data.
//
// Fixtures are generated from a fixed seed so they are byte-stable across runs.

import type { BrainModel, Connection, Neuron, NeuronType } from "../brainModel";
import { mulberry32 } from "../prng";

/** Build neurons with deterministic biases. Ids are contiguous:
 *  senses [0, S), internals [S, S+H), actions [S+H, S+H+A). */
function makeNeurons(
    senseCount: number,
    internalCount: number,
    actionCount: number,
    rand: () => number,
): Neuron[] {
    const neurons: Neuron[] = [];
    const push = (count: number, type: NeuronType) => {
        for (let i = 0; i < count; i++) {
            neurons.push({
                id: neurons.length,
                type,
                // Bias in [-1, 1], deterministic.
                bias: Math.round((rand() * 2 - 1) * 1000) / 1000,
            });
        }
    };
    push(senseCount, "sense");
    push(internalCount, "internal");
    push(actionCount, "action");
    return neurons;
}

/**
 * Deterministically generate a brain. Sources are senses + internals; sinks are
 * internals + actions. A configurable share of connections are forced to be
 * internal→internal (recurrent), and a few of those are self-loops, so the
 * recurrent structure is always exercised.
 */
function makeBrain(
    senseCount: number,
    internalCount: number,
    actionCount: number,
    targetConnections: number,
    seed: number,
): BrainModel {
    const rand = mulberry32(seed);
    const neurons = makeNeurons(senseCount, internalCount, actionCount, rand);

    const senseIds = neurons.filter((n) => n.type === "sense").map((n) => n.id);
    const internalIds = neurons
        .filter((n) => n.type === "internal")
        .map((n) => n.id);
    const actionIds = neurons
        .filter((n) => n.type === "action")
        .map((n) => n.id);

    const sourceIds = [...senseIds, ...internalIds];
    const sinkIds = [...internalIds, ...actionIds];
    const pick = (ids: number[]) => ids[Math.floor(rand() * ids.length)];

    // De-duplicate (source, sink) pairs so a connection count is meaningful.
    const seen = new Set<string>();
    const connections: Connection[] = [];
    const add = (source: number, sink: number) => {
        const key = `${source}->${sink}`;
        if (seen.has(key)) return;
        seen.add(key);
        // Signed weight in [-4, 4], deterministic, biased away from zero so the
        // |weight|→stroke-width mapping has visible range.
        const mag = 0.2 + rand() * 3.8;
        const weight = Math.round((rand() < 0.5 ? -mag : mag) * 1000) / 1000;
        connections.push({ source, sink, weight });
    };

    // Guarantee a handful of self-loops when there are internal neurons.
    if (internalIds.length > 0) {
        const selfLoops = Math.min(internalIds.length, 3);
        for (let i = 0; i < selfLoops; i++) add(internalIds[i], internalIds[i]);
    }

    let guard = targetConnections * 20;
    while (connections.length < targetConnections && guard-- > 0) {
        // ~35% of attempts are forced recurrent internal→internal edges.
        if (internalIds.length > 1 && rand() < 0.35) {
            add(pick(internalIds), pick(internalIds));
        } else {
            add(pick(sourceIds), pick(sinkIds));
        }
    }

    return { neurons, connections };
}

/** Tiny brain (6 sense × 4 internal × 3 action) — exercises Signature diagram
 *  mode and a legible Explorer. */
export const tinyBrain: BrainModel = makeBrain(6, 4, 3, 14, 0x7a1c);

/** Medium brain (10 sense × 20 internal × 5 action) — crosses the diagram→
 *  fingerprint threshold in the Signature. */
export const mediumBrain: BrainModel = makeBrain(10, 20, 5, 80, 0x51d2);

/** Maximum brain (20 sense × 128 internal × 15 action, 1000+ connections) —
 *  the Explorer stress case and the anti-hairball default trigger. */
export const maxBrain: BrainModel = makeBrain(20, 128, 15, 1100, 0x9e3f);

/** All fixtures keyed by a short label, for component stories/tests. */
export const brainFixtures = {
    tiny: tinyBrain,
    medium: mediumBrain,
    max: maxBrain,
} as const;

export type BrainFixtureName = keyof typeof brainFixtures;

const FIXTURE_CYCLE = [tinyBrain, mediumBrain, maxBrain];

/**
 * Pick a fixture brain for an agent id. Single seam shared by the Signature and
 * the Explorer so both show the same brain for a given agent.
 */
export function brainForAgent(agentId: number): BrainModel {
    // TODO: replace with the agent's decoded brain once the worker exposes
    // per-neuron / per-connection data on AgentInfo.
    const i =
        ((agentId % FIXTURE_CYCLE.length) + FIXTURE_CYCLE.length) %
        FIXTURE_CYCLE.length;
    return FIXTURE_CYCLE[i];
}
