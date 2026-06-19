// Pure math helpers for the kinematic sculpture and grid-mode rendering.
// No side effects; no DOM or canvas access. Importable from both the main
// thread and the Web Worker.
//
// ── Sculpture-authoring contract ───────────────────────────────────────────────
// A "sculpture" is a pure function with the `SculptureFn` shape: given an agent
// index, the population size, and a `KinematicCtx`, it returns that agent's
// canvas-pixel centre `{x, y}`, dot radius `r`, and dot `opacity`. The contract:
//   • Deterministic per index: the same (i, pop, ctx) MUST return the same point.
//     No randomness, no module-level mutable state — only `ctx.t` drives motion.
//   • Output is in canvas pixels; sculptures may place dots beyond the viewport
//     edges (cells span a margin) but should keep them near the canvas.
//   • `opacity` is in `[0, 1]`; the worker draws each dot at that alpha, so
//     sculptures use it for depth (e.g. far-side points fade) or wave shading.
//   • `ctx.beat` (0..1) and `ctx.pointer` are reserved for the beat response:
//     `beat = 0` (and/or `pointer = null`) MUST render the neutral, un-pulsed
//     sculpture. Callers that don't drive beats pass `beat: 0, pointer: null`.
// To swap the active sculpture, change the function the worker calls in its
// `drawKinematic` / `drawTransitionIn` draw loops (`sim.worker.ts`) from
// `kinematicPosition` to `sphereSculpture` (or any other `SculptureFn`). The
// signature is frozen, so swapping is a one-line change.

/** Context passed to every `SculptureFn`. Field set is frozen (issue #80). */
export interface KinematicCtx {
    /** Seconds since the worker's kinematic epoch. */
    t: number;
    canvasW: number;
    canvasH: number;
    /** Full-viewport beat intensity, 0..1 (0 = no beat). */
    beat: number;
    /** Beat epicentre in canvas px, or null for a uniform (full-viewport) beat. */
    pointer: { x: number; y: number } | null;
}

/** A swappable sculpture: agent index + population + context → dot placement.
 *  `opacity` is in `[0, 1]`; the worker renders each dot at that alpha. */
export type SculptureFn = (
    i: number,
    pop: number,
    ctx: KinematicCtx,
) => { x: number; y: number; r: number; opacity: number };

// ── Wave-surface sculpture (the default idle sculpture) ─────────────────────────

/** Fraction of each canvas dimension the barycenter matrix extends beyond the
 *  viewport, so the surface never reveals an edge seam. */
const MARGIN = 0.12;
/** Vertical wave displacement, as a fraction of canvas height. */
const AMP_Y = 0.045;
/** Horizontal sway, as a fraction of canvas width. */
const AMP_X = 0.02;
/** Base dot radius (px) and how far it swings with the wave field (px). */
const R_BASE = 1.8;
const R_SWING = 0.8;
/** Opacity shading: dim in wave troughs, bright on crests → [BASE-SWING, BASE+SWING]. */
const OPACITY_BASE = 0.7;
const OPACITY_SWING = 0.3;
/** Spatial frequencies (radians across the normalised viewport) of the two
 *  superposed travelling waves, and their temporal speeds (radians/second). */
const WAVE_A = { kx: 6.0, ky: 4.0, omega: 1.1 };
const WAVE_B = { kx: 3.0, ky: -5.0, omega: 0.7 };
/** Beat gain: how much a full-intensity beat amplifies displacement and radius. */
const BEAT_AMP_GAIN = 1.6;
const BEAT_R_GAIN = 1.5;
/** Click-beat falloff: epicentre influence radius, as a fraction of the canvas
 *  diagonal. Dormant while `beat = 0`. */
const BEAT_SIGMA = 0.15;

/** Near-square (col, row) layout for `pop` cells over a `canvasW`×`canvasH` box. */
function waveGrid(
    pop: number,
    canvasW: number,
    canvasH: number,
): { cols: number; rows: number } {
    const aspect = canvasH > 0 ? canvasW / canvasH : 1;
    const cols = Math.max(1, Math.ceil(Math.sqrt(pop * aspect)));
    const rows = Math.max(1, Math.ceil(pop / cols));
    return { cols, rows };
}

/** Brighten an opacity toward fully opaque by the local beat intensity. */
function liftByBeat(opacity: number, beat: number): number {
    return opacity + (1 - opacity) * beat;
}

/** Beat intensity felt at a cell, given the context. Uniform for full-viewport
 *  beats; a Gaussian falloff from `ctx.pointer` for click beats. */
function beatAt(ctx: KinematicCtx, x: number, y: number): number {
    if (ctx.beat <= 0) return 0;
    if (!ctx.pointer) return ctx.beat;
    const diag = Math.hypot(ctx.canvasW, ctx.canvasH) || 1;
    const dist = Math.hypot(x - ctx.pointer.x, y - ctx.pointer.y) / diag;
    const falloff = Math.exp(-(dist * dist) / (2 * BEAT_SIGMA * BEAT_SIGMA));
    return ctx.beat * falloff;
}

/**
 * Default idle sculpture: a wave-like surface. Each agent owns a barycenter on a
 * near-square matrix that spans the viewport plus a {@link MARGIN}; cells
 * oscillate around it with two superposed travelling waves, giving a surface
 * (rather than orbit) feel. Deterministic per index. A beat amplifies the
 * displacement and radius and brightens the dot (uniform, or decaying from
 * `ctx.pointer`).
 */
export function kinematicPosition(
    i: number,
    pop: number,
    ctx: KinematicCtx,
): { x: number; y: number; r: number; opacity: number } {
    const { t, canvasW, canvasH } = ctx;
    const { cols, rows } = waveGrid(pop, canvasW, canvasH);
    const col = i % cols;
    const row = Math.floor(i / cols);

    // Barycenter across the margin-extended box.
    const spanW = canvasW * (1 + 2 * MARGIN);
    const spanH = canvasH * (1 + 2 * MARGIN);
    const bx = -MARGIN * canvasW + ((col + 0.5) / cols) * spanW;
    const by = -MARGIN * canvasH + ((row + 0.5) / rows) * spanH;

    // Two superposed travelling waves over normalised barycenter coords.
    const nx = canvasW > 0 ? bx / canvasW : 0;
    const ny = canvasH > 0 ? by / canvasH : 0;
    const phaseA = nx * WAVE_A.kx + ny * WAVE_A.ky - t * WAVE_A.omega;
    const phaseB = nx * WAVE_B.kx + ny * WAVE_B.ky - t * WAVE_B.omega;
    const wave = (Math.sin(phaseA) + Math.sin(phaseB)) * 0.5; // [-1, 1]
    const sway = Math.sin(phaseB); // [-1, 1]

    const beat = beatAt(ctx, bx, by);
    const ampFactor = 1 + BEAT_AMP_GAIN * beat;

    return {
        x: bx + AMP_X * canvasW * sway,
        y: by + AMP_Y * canvasH * wave - ampFactor * AMP_X * canvasW,
        r: R_BASE + R_SWING * wave + BEAT_R_GAIN * beat,
        opacity: liftByBeat(OPACITY_BASE + OPACITY_SWING * wave, beat),
    };
}

// ── Beat envelope ───────────────────────────────────────────────────────────────

/** Sharp attack then smooth decay, in milliseconds. */
const ATTACK_MS = 90;
const DECAY_MS = 700;

/**
 * Beat transfer function in `[0, 1]`: a sharp linear attack to a peak of `1` at
 * {@link ATTACK_MS}, then a smooth monotonic decay (via {@link easeInOut}) back
 * to `0` at `ATTACK_MS + DECAY_MS`. Returns `0` outside that window. Pure;
 * consumed by the worker to drive `ctx.beat`.
 */
export function beatEnvelope(elapsedMs: number): number {
    if (elapsedMs <= 0) return 0;
    if (elapsedMs < ATTACK_MS) return elapsedMs / ATTACK_MS;
    const decayed = elapsedMs - ATTACK_MS;
    if (decayed < DECAY_MS) return 1 - easeInOut(decayed / DECAY_MS);
    return 0;
}

// ── Sphere sample (swappable alternative sculpture) ─────────────────────────────

/** Golden angle, for an even Fibonacci-sphere point distribution. */
const GOLDEN_ANGLE = Math.PI * (3 - Math.sqrt(5));
/** Sphere rotation speed (radians/second) about the vertical axis. */
const SPHERE_TURN = 0.5;
/** Opacity of the farthest-back sphere point; the front face reaches 1. */
const SPHERE_OPACITY_MIN = 0.3;

/**
 * Sample sculpture: a rotating 3-D sphere projected to 2-D. Points are placed
 * deterministically per index with a Fibonacci-sphere distribution, rotated
 * about the vertical axis by `ctx.t`, and perspective-projected (the projection
 * math is ported from the issue's `sphere.js` reference — its stateful particle
 * lifecycle is intentionally dropped). Far-side points fade (lower opacity) for
 * a depth cue. Swap it in for {@link kinematicPosition} to author a different
 * idle sculpture. Honours beats via the same envelope.
 */
export function sphereSculpture(
    i: number,
    pop: number,
    ctx: KinematicCtx,
): { x: number; y: number; r: number; opacity: number } {
    const { t, canvasW, canvasH } = ctx;
    const cx = canvasW / 2;
    const cy = canvasH / 2;
    const sphereRad = Math.min(canvasW, canvasH) * 0.35;

    // Fibonacci-sphere point for index i (deterministic, evenly spread).
    const y0 = pop > 1 ? 1 - (i / (pop - 1)) * 2 : 0; // 1 → -1
    const ring = Math.sqrt(Math.max(0, 1 - y0 * y0));
    const theta = GOLDEN_ANGLE * i;
    const px = Math.cos(theta) * ring * sphereRad;
    const pz = Math.sin(theta) * ring * sphereRad;
    const py = y0 * sphereRad;

    // Rotate about the vertical (y) axis.
    const ang = t * SPHERE_TURN;
    const sin = Math.sin(ang);
    const cos = Math.cos(ang);
    const rotX = cos * px + sin * pz;
    const rotZ = -sin * px + cos * pz;

    // Perspective projection: camera in front of the sphere looking toward -z.
    const fLen = sphereRad * 3;
    const m = fLen / (fLen + sphereRad - rotZ);

    const beat = beatAt(ctx, cx + rotX * m, cy + py * m);
    // Depth in [-1, 1] (front toward camera = +1) → opacity floor..1.
    const depth = rotZ / sphereRad;
    const shade =
        SPHERE_OPACITY_MIN + (1 - SPHERE_OPACITY_MIN) * (depth * 0.5 + 0.5);
    return {
        x: cx + rotX * m,
        y: cy + py * m,
        r: (R_BASE + R_SWING) * m + BEAT_R_GAIN * beat,
        opacity: liftByBeat(shade, beat),
    };
}

// ── Grid-mode helpers (unchanged; reused by transitions) ────────────────────────

/** Canvas-pixel centre + radius for a simulation grid cell (gx, gy).
 *  Supports rectangular grids: gridW/gridH are the pixel dimensions of the
 *  grid region on the canvas; gridCellsX/Y are the simulation cell counts. */
export function gridPosition(
    gx: number,
    gy: number,
    gridX: number,
    gridY: number,
    gridW: number,
    gridH: number,
    gridCellsX: number,
    gridCellsY: number,
): { x: number; y: number; r: number } {
    const cellPxW = gridW / gridCellsX;
    const cellPxH = gridH / gridCellsY;
    return {
        x: gridX + (gx + 0.5) * cellPxW,
        y: gridY + (gy + 0.5) * cellPxH,
        r: Math.max(1.5, Math.min(cellPxW, cellPxH) * 0.4),
    };
}

/** Linear interpolation between two 2-D points. */
export function lerpVec2(
    a: { x: number; y: number },
    b: { x: number; y: number },
    t: number,
): { x: number; y: number } {
    return { x: a.x + (b.x - a.x) * t, y: a.y + (b.y - a.y) * t };
}

/** Smooth-step (quadratic ease-in-out), t ∈ [0, 1]. */
export function easeInOut(t: number): number {
    return t < 0.5 ? 2 * t * t : -1 + (4 - 2 * t) * t;
}
