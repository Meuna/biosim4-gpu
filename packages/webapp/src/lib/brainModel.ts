// Framework-agnostic brain (neural network) model for an agent.
// No DOM, no Svelte, no canvas — importable from anywhere. Mirrors the
// project's logic/rendering separation: this module owns the *data*; layout
// (brainLayout.ts) and the renderers (BrainSignature/BrainExplorer) consume it.
//
// A biosim4 brain is NOT a clean feed-forward net. It decodes from the genome
// into three kinds of neuron and a set of weighted connections:
//   - sense    (up to 20):  sources only.
//   - internal (up to 128): both source and sink; recurrent, incl. self-loops.
//   - action   (up to 15):  sinks only.
// Connections are (source, sink, weight) with a signed float weight.

export type NeuronType = "sense" | "internal" | "action";

export interface Neuron {
    /** Stable identifier, unique within a BrainModel. Used for layout, focus,
     *  hit-testing and search. Kept stable so a future edit layer can address
     *  neurons without re-keying. */
    id: number;
    type: NeuronType;
    /** Decoded bias for the neuron. Also the documented seam for phase-2 live
     *  activation binding — a renderer can later read a parallel activation
     *  value here without changing the model shape. */
    // TODO(phase2): bind live per-neuron activation alongside this static bias.
    bias: number;
}

export interface Connection {
    /** Source neuron id (a sense or internal neuron). */
    source: number;
    /** Sink neuron id (an internal or action neuron). */
    sink: number;
    /** Signed connection weight; sign encodes excitatory (+) / inhibitory (−). */
    weight: number;
}

/** The complete decoded brain. Structured (stable ids, no positional data baked
 *  in) so a future drag-to-edit ticket can mutate `connections` in place and a
 *  layout pass can be re-run without touching this model. */
export interface BrainModel {
    neurons: Neuron[];
    connections: Connection[];
}

/** The four connection kinds, by the types of the endpoints. */
export type ConnectionKind =
    | "sense-internal"
    | "sense-action"
    | "internal-internal"
    | "internal-action";

/** Aggregate counts used by the Signature header line and the render-mode and
 *  anti-hairball thresholds. */
export interface BrainCounts {
    sense: number;
    internal: number;
    action: number;
    /** Total neuron count (sense + internal + action). */
    nodes: number;
    connections: number;
}

/** Count neurons by type plus total nodes and connections. */
export function brainCounts(model: BrainModel): BrainCounts {
    let sense = 0;
    let internal = 0;
    let action = 0;
    for (const n of model.neurons) {
        if (n.type === "sense") sense++;
        else if (n.type === "internal") internal++;
        else action++;
    }
    return {
        sense,
        internal,
        action,
        nodes: model.neurons.length,
        connections: model.connections.length,
    };
}

/** Classify a connection by its endpoint types. A connection whose source and
 *  sink are the same internal neuron is still "internal-internal" (a self-loop).
 *  Returns null if either endpoint id is unknown. */
export function connectionKind(
    model: BrainModel,
    conn: Connection,
): ConnectionKind | null {
    const src = model.neurons.find((n) => n.id === conn.source);
    const sink = model.neurons.find((n) => n.id === conn.sink);
    if (!src || !sink) return null;
    if (src.type === "sense" && sink.type === "internal")
        return "sense-internal";
    if (src.type === "sense" && sink.type === "action") return "sense-action";
    if (src.type === "internal" && sink.type === "internal")
        return "internal-internal";
    return "internal-action";
}

/** True when source and sink are the same neuron (a recurrent self-loop). */
export function isSelfLoop(conn: Connection): boolean {
    return conn.source === conn.sink;
}

/**
 * Build a BrainModel from a selected agent's decoded brain.
 *
 * The worker (`sim.worker.ts`, `AgentInfo`) does not yet expose per-neuron or
 * per-connection data, so callers currently pass already-decoded arrays (e.g.
 * from a fixture). This function exists so renderers never construct the model
 * themselves and so the eventual real wiring has a single seam.
 */
export function buildBrainModel(
    neurons: Neuron[],
    connections: Connection[],
): BrainModel {
    // TODO: wire from the decoded agent brain once the worker exposes
    // per-neuron / per-connection data on AgentInfo. Until then callers supply
    // fixture data (see lib/brain/fixture.ts). Do NOT synthesise data here.
    return { neurons, connections };
}
