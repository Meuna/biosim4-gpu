// Pure, deterministic layout for a BrainModel. No DOM, no canvas, no Svelte.
//
// Same input → identical output every run: the only source of "randomness" is a
// fixed-seed PRNG, and the relaxation runs a fixed iteration count (not
// convergence-based). Positions are returned in a normalised [0, 1] × [0, 1]
// space; renderers map that to their own viewport.
//
// Layout shape (the topology is recurrent, NOT feed-forward):
//   - sense neurons  → pinned left column.
//   - action neurons → pinned right column.
//   - internal neurons → a central 2D cluster placed by a seeded barycentric
//     relaxation (attraction to connected neighbours + mild mutual repulsion)
//     to reduce edge crossings without assuming clean layers.

import type { BrainModel, Connection } from "./brainModel";
import { isSelfLoop } from "./brainModel";
import { mulberry32 } from "./prng";

export interface Point {
    x: number;
    y: number;
}

/** Edge routing class for the renderers. forward = left→right (gentle curve);
 *  recurrent = sideways/feedback (visibly bowed); self = self-loop (small arc). */
export type EdgeRouting = "forward" | "recurrent" | "self";

export interface LaidOutEdge {
    source: number;
    sink: number;
    weight: number;
    routing: EdgeRouting;
    /** Signed perpendicular bow offset in layout units. Renderers use it as the
     *  control-point displacement for the curve; unused for self-loops. */
    bow: number;
}

export interface BrainLayout {
    /** Neuron id → normalised position. */
    positions: Map<number, Point>;
    edges: LaidOutEdge[];
    /** Always the unit square for this implementation; exposed for renderers. */
    bounds: { minX: number; minY: number; maxX: number; maxY: number };
}

// ── Layout constants ──────────────────────────────────────────────────────
const SENSE_X = 0.05;
const ACTION_X = 0.95;
const INTERNAL_X_MIN = 0.28;
const INTERNAL_X_MAX = 0.72;
const Y_MIN = 0.05;
const Y_MAX = 0.95;
// Fixed seed + fixed iteration count → deterministic, bounded-cost relaxation.
const LAYOUT_SEED = 0x1f2e3d4c;
const RELAX_ITERATIONS = 220;
const ATTRACT = 0.1;
const REPULSE = 0.0006;
const FORWARD_BOW = 0.03;
const RECURRENT_BOW = 0.12;

/** Evenly distribute neuron ids down a pinned column at a fixed x. */
function placeColumn(ids: number[], x: number, positions: Map<number, Point>) {
    const n = ids.length;
    for (let i = 0; i < n; i++) {
        // Centre a single node; otherwise spread across [Y_MIN, Y_MAX].
        const t = n === 1 ? 0.5 : i / (n - 1);
        positions.set(ids[i], { x, y: Y_MIN + t * (Y_MAX - Y_MIN) });
    }
}

/** Seed internal positions deterministically inside the central band. */
function initInternal(ids: number[], rand: () => number): Map<number, Point> {
    const pts = new Map<number, Point>();
    for (const id of ids) {
        pts.set(id, {
            x: INTERNAL_X_MIN + rand() * (INTERNAL_X_MAX - INTERNAL_X_MIN),
            y: Y_MIN + rand() * (Y_MAX - Y_MIN),
        });
    }
    return pts;
}

const clamp = (v: number, lo: number, hi: number) =>
    v < lo ? lo : v > hi ? hi : v;

/** Average position of every neuron connected to `id` (either direction). */
function neighbourBarycentre(
    id: number,
    neighbours: number[],
    fixed: Map<number, Point>,
    internal: Map<number, Point>,
): Point | null {
    let sx = 0;
    let sy = 0;
    let count = 0;
    for (const other of neighbours) {
        const p = fixed.get(other) ?? internal.get(other);
        if (!p) continue;
        sx += p.x;
        sy += p.y;
        count++;
    }
    return count === 0 ? null : { x: sx / count, y: sy / count };
}

/** Mutual repulsion of an internal node from all other internal nodes. */
function repulsion(
    id: number,
    pos: Point,
    internal: Map<number, Point>,
): Point {
    let fx = 0;
    let fy = 0;
    for (const [otherId, q] of internal) {
        if (otherId === id) continue;
        const dx = pos.x - q.x;
        const dy = pos.y - q.y;
        const d2 = dx * dx + dy * dy + 1e-4;
        fx += (REPULSE * dx) / d2;
        fy += (REPULSE * dy) / d2;
    }
    return { x: fx, y: fy };
}

/** Fixed-iteration barycentric relaxation (Jacobi update for order-independence
 *  and determinism). */
function relax(
    internalIds: number[],
    neighbourMap: Map<number, number[]>,
    fixed: Map<number, Point>,
    internal: Map<number, Point>,
) {
    for (let iter = 0; iter < RELAX_ITERATIONS; iter++) {
        const next = new Map<number, Point>();
        for (const id of internalIds) {
            const pos = internal.get(id)!;
            const bary = neighbourBarycentre(
                id,
                neighbourMap.get(id) ?? [],
                fixed,
                internal,
            );
            const rep = repulsion(id, pos, internal);
            const tx = bary ? pos.x + ATTRACT * (bary.x - pos.x) : pos.x;
            const ty = bary ? pos.y + ATTRACT * (bary.y - pos.y) : pos.y;
            next.set(id, {
                x: clamp(tx + rep.x, INTERNAL_X_MIN, INTERNAL_X_MAX),
                y: clamp(ty + rep.y, Y_MIN, Y_MAX),
            });
        }
        for (const [id, p] of next) internal.set(id, p);
    }
}

/** Build an undirected neighbour list per neuron from the connections. */
function buildNeighbourMap(model: BrainModel): Map<number, number[]> {
    const map = new Map<number, number[]>();
    const add = (a: number, b: number) => {
        const list = map.get(a) ?? [];
        list.push(b);
        map.set(a, list);
    };
    for (const c of model.connections) {
        if (c.source === c.sink) continue; // self-loops don't pull the node
        add(c.source, c.sink);
        add(c.sink, c.source);
    }
    return map;
}

/** Classify routing + bow for one connection given final positions. */
function routeEdge(
    conn: Connection,
    positions: Map<number, Point>,
): LaidOutEdge {
    if (isSelfLoop(conn)) {
        return { ...conn, routing: "self", bow: 0 };
    }
    const a = positions.get(conn.source);
    const b = positions.get(conn.sink);
    // Sideways or backward edges read as recurrent/feedback.
    const recurrent = !a || !b || b.x <= a.x;
    const sign = (conn.source + conn.sink) % 2 === 0 ? 1 : -1;
    return {
        ...conn,
        routing: recurrent ? "recurrent" : "forward",
        bow: sign * (recurrent ? RECURRENT_BOW : FORWARD_BOW),
    };
}

/**
 * Compute a deterministic layout for a brain. Pure: no globals, no time, no
 * Math.random. Calling it twice on the same model yields identical positions.
 */
export function layoutBrain(model: BrainModel): BrainLayout {
    const positions = new Map<number, Point>();
    const senseIds = model.neurons
        .filter((n) => n.type === "sense")
        .map((n) => n.id);
    const actionIds = model.neurons
        .filter((n) => n.type === "action")
        .map((n) => n.id);
    const internalIds = model.neurons
        .filter((n) => n.type === "internal")
        .map((n) => n.id);

    placeColumn(senseIds, SENSE_X, positions);
    placeColumn(actionIds, ACTION_X, positions);

    const rand = mulberry32(LAYOUT_SEED);
    const internal = initInternal(internalIds, rand);
    relax(internalIds, buildNeighbourMap(model), positions, internal);
    for (const [id, p] of internal) positions.set(id, p);

    const edges = model.connections.map((c) => routeEdge(c, positions));

    return {
        positions,
        edges,
        bounds: { minX: 0, minY: 0, maxX: 1, maxY: 1 },
    };
}
