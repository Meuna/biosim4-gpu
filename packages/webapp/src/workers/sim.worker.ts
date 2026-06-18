// biosim.mjs is a pre-built Emscripten ES6 module. @sim-wasm resolves to the
// sim-wasm build output directory (outside publicDir), so Vite's ?url import
// is accepted. locateFile pins biosim.wasm to its publicDir URL independently
// of where biosim.mjs is served (it gets a hashed URL in production builds).
import biosimUrl from "@sim-wasm/biosim.mjs?url";
import {
    easeInOut,
    gridPosition,
    kinematicPosition,
    lerpVec2,
} from "../lib/kinematic";
import { unpackConn, type BrainConn } from "../lib/brain";
import { stepDelay, createFpsWindow } from "../lib/playbackRate";

// ── Types & Protocol ──────────────────────────────────────────────────────────

interface EmscriptenModule {
    ccall(
        name: string,
        returnType: "number" | null,
        argTypes: string[],
        args: unknown[],
    ): number;
    HEAP32: Int32Array;
    HEAPU8: Uint8Array;
    HEAPU32: Uint32Array;
}

interface EmscriptenOptions {
    locateFile?: (path: string) => string;
}

type EmscriptenFactory = (
    opts?: EmscriptenOptions,
) => Promise<EmscriptenModule>;

/** Barrier kind — mirrors biosim_barrier_kind_t in barriers.h. */
export type BarrierKind = "hbar" | "vbar" | "square" | "circle" | "corner";

/** Corner arm directions — mirrors biosim_corner_quadrant_t in barriers.h.
 *  Cardinals follow the io_defs.h direction table (north is -y):
 *  ne → +x/-y, nw → -x/-y, se → +x/+y, sw → -x/+y. */
export type CornerQuadrant = "ne" | "nw" | "se" | "sw";

/** One barrier shape — mirrors biosim_barrier_spec_t in barriers.h.
 *  Positional fields are fractions of the grid dimension ([0, 1]);
 *  null means "pick randomly" (maps to the BIOSIM_BARRIER_*_UNSET sentinel).
 *  quadrant applies to "corner" only and defaults to "ne" elsewhere. */
export interface BarrierSpec {
    kind: BarrierKind;
    x: number | null;
    y: number | null;
    length: number | null;
    width: number | null;
    quadrant: CornerQuadrant;
}

/** Challenge specification — mirrors biosim_challenge_spec_t in challenge_spec.h. */
export type ChallengeSpec =
    | { kind: "x_band"; xMin: number; xMax: number; mirror: boolean }
    | { kind: "disc"; x: number; y: number; radius: number; weighted: boolean }
    | { kind: "corners"; radius: number; weighted: boolean }
    | {
          kind: "neighbor_count";
          radius: number;
          minN: number;
          maxN: number;
          excludeBorder: boolean;
      }
    | {
          kind: "center_sparse";
          x: number;
          y: number;
          outerR: number;
          innerR: number;
          minN: number;
          maxN: number;
          weighted: boolean;
      }
    | { kind: "against_wall" }
    | { kind: "migrate_distance" }
    | { kind: "touch_any_wall" }
    | { kind: "radioactive_walls" }
    | { kind: "pairs" }
    | { kind: "location_sequence"; radius: number }
    | { kind: "near_barrier"; radius: number }
    | { kind: "altruism" };

/** All per-agent state fields readable from the WASM heap. */
export interface AgentInfo {
    id: number;
    alive: boolean;
    gx: number;
    gy: number;
    birthX: number;
    birthY: number;
    heading: number;
    oscPeriod: number;
    responsiveness: number;
    losRange: number;
    challengeBits: number;
    fingerprint: string;
}

/** Scalar parameters that can be configured before (re-)initialising the sim. */
export interface SimParams {
    population: number;
    gridSizeX: number;
    gridSizeY: number;
    stepsPerGen: number;
    maxGenes: number;
    maxNeurons: number;
    pointMutationRate: number;
    sexualReproduction: boolean;
    chooseParentsByFitness: boolean;
    losRange: number;
    sensorRadius: number;
    enableKill: boolean;
    responsivenessCurveK: number;
    challenge: ChallengeSpec;
    barriers: BarrierSpec[];
}

export type WorkerCmd =
    | { type: "play" }
    | { type: "stop" }
    | { type: "reset" }
    | { type: "step" }
    | { type: "rewind" }
    | { type: "rewindConfigured"; params: SimParams }
    | { type: "clearGenom" }
    | { type: "nextGeneration" }
    | { type: "nextGenerationConfigured"; params: SimParams }
    | { type: "configure"; params: SimParams }
    | {
          type: "canvas";
          canvas: OffscreenCanvas;
          overlayColor: string;
          borderColor: string;
          accentColor: string;
      }
    | {
          type: "layout";
          canvasW: number;
          canvasH: number;
          gridX: number;
          gridY: number;
          gridW: number;
          gridH: number;
      }
    | {
          type: "pickAgentAtCell";
          gx: number;
          gy: number;
          reason: "click" | "hover";
      }
    | { type: "navigateAgent"; fromId: number; direction: -1 | 1 }
    | { type: "randomAgent" }
    | { type: "selectAgentById"; id: number }
    | { type: "selectAgent"; id: number | null }
    | { type: "hoverAgent"; id: number | null }
    | { type: "requestBrain"; id: number }
    | { type: "setSpeed"; fps: number }
    | { type: "startFreeRun" }
    | { type: "stopFreeRun" }
    | { type: "exportSnapshot" }
    | { type: "loadSnapshot"; data: Uint8Array; rewindFirst: boolean };

export type WorkerEvent =
    | { type: "ready" }
    | { type: "stepped"; gen: number; step: number }
    | { type: "genComplete"; gen: number; step: number }
    | { type: "paused"; gen: number; step: number }
    | {
          type: "census";
          gen: number;
          population: number;
          survivors: number;
          kills: number;
          requiredGenomeLen: number;
          requiredNeurons: number;
      }
    | {
          type: "configured";
          population: number;
          gridSizeX: number;
          gridSizeY: number;
          stepsPerGen: number;
      }
    | {
          type: "rewindConfigured";
          gen: number;
          population: number;
          gridSizeX: number;
          gridSizeY: number;
          stepsPerGen: number;
      }
    | {
          type: "nextGenerationConfigured";
          gen: number;
          population: number;
          gridSizeX: number;
          gridSizeY: number;
          stepsPerGen: number;
          censusPopulation: number;
          survivors: number;
          kills: number;
          requiredGenomeLen: number;
          requiredNeurons: number;
      }
    | { type: "error"; message: string; call?: string; fatal?: boolean }
    | { type: "agentPicked"; reason: "click" | "hover"; info: AgentInfo }
    | { type: "agentMissed"; reason: "click" | "hover" }
    | { type: "agentUpdated"; info: AgentInfo }
    | {
          type: "brainData";
          id: number;
          conns: BrainConn[];
          neuronCount: number;
      }
    | { type: "fps"; value: number }
    | { type: "snapshotData"; data: Uint8Array }
    | {
          type: "snapshotLoaded";
          gen: number;
          population: number;
          requiredGenomeLen: number;
          requiredNeurons: number;
      };

interface Layout {
    canvasW: number;
    canvasH: number;
    gridX: number;
    gridY: number;
    gridW: number;
    gridH: number;
    gridCellsX: number;
    gridCellsY: number;
}

type Mode = "idle" | "transitioning-in" | "running";

// ── WASM Bindings ─────────────────────────────────────────────────────────────

let biosim: EmscriptenModule | null = null;

function call(name: string): number {
    return biosim!.ccall(name, "number", [], []);
}

// Runs a status-returning WASM call. On a non-zero status it logs the failed
// call name to the console and forwards an error event to the main thread, then
// returns the status so the caller can bail before posting a success reply.
// `fatal` marks unrecoverable failures (e.g. the bootstrap init) that warrant a
// blocking display rather than the auto-dismissing banner.
function callChecked(name: string, fatal = false): number {
    const rc = call(name);
    if (rc !== 0) {
        console.error(`WASM call ${name} returned status ${rc}`);
        postMessage({
            type: "error",
            message: `Simulation call "${name}" failed (status ${rc})`,
            call: name,
            fatal,
        } satisfies WorkerEvent);
    }
    return rc;
}

function setParamInt(name: string, val: number): void {
    biosim!.ccall(
        "biosim_wasm_set_param_int",
        "number",
        ["string", "number"],
        [name, val],
    );
}

function setParamFloat(name: string, val: number): void {
    biosim!.ccall(
        "biosim_wasm_set_param_float",
        "number",
        ["string", "number"],
        [name, val],
    );
}

function setParamBool(name: string, val: boolean): void {
    biosim!.ccall(
        "biosim_wasm_set_param_bool",
        "number",
        ["string", "number"],
        [name, val ? 1 : 0],
    );
}

// ── Barriers ──────────────────────────────────────────────────────────────────

// Maps BarrierSpec.kind strings to biosim_barrier_kind_t ordinals (barriers.h).
const BARRIER_KIND_INT: Record<BarrierKind, number> = {
    hbar: 0,
    vbar: 1,
    square: 2,
    circle: 3,
    corner: 4,
};

// Maps CornerQuadrant strings to biosim_corner_quadrant_t ordinals (barriers.h).
const CORNER_QUADRANT_INT: Record<CornerQuadrant, number> = {
    ne: 0,
    nw: 1,
    se: 2,
    sw: 3,
};

// Cached barrier cell coordinates: flat [gx0,gy0, gx1,gy1, ...].
// Populated after biosim_wasm_init so we read the authoritative grid once per configure.
let barrierCells: Int32Array | null = null;
// Tiled diagonal-line (///) pattern for barrier cells; created once after ctx is ready.
let barrierHatchPattern: CanvasPattern | null = null;

// Passes barrier specs to WASM. The C core stores grid ratios in [0, 1]
// directly, so fractions pass straight through; null maps to the sentinel
// (-1.0 for position, 0.0 for dimension).
function setBarriers(specs: BarrierSpec[]): void {
    biosim!.ccall("biosim_wasm_clear_barriers", null, [], []);
    for (const spec of specs) {
        const x = spec.x !== null ? spec.x : -1.0;
        const y = spec.y !== null ? spec.y : -1.0;
        const len = spec.length !== null ? spec.length : 0.0;
        const w = spec.width !== null ? spec.width : 0.0;
        biosim!.ccall(
            "biosim_wasm_add_barrier",
            "number",
            ["number", "number", "number", "number", "number", "number"],
            [
                BARRIER_KIND_INT[spec.kind],
                x,
                y,
                len,
                w,
                CORNER_QUADRANT_INT[spec.quadrant],
            ],
        );
    }
}

// Scans the grid for BIOSIM_GRID_BARRIER cells (0xFFFFFFFF) and caches their
// coordinates. Called once after biosim_wasm_init so rendering reuses the cache.
function cacheBarrierCells(): void {
    if (!biosim || !layout) {
        barrierCells = null;
        return;
    }
    const { gridCellsX: W, gridCellsY: H } = layout;
    const cellsOff = call("biosim_wasm_get_grid_cells_ptr") >>> 2;
    const { HEAPU32 } = biosim;
    const tmp: number[] = [];
    for (let y = 0; y < H; y++) {
        for (let x = 0; x < W; x++) {
            if (HEAPU32[cellsOff + y * W + x] === 0xffffffff) {
                tmp.push(x, y);
            }
        }
    }
    barrierCells = new Int32Array(tmp);
}

// Builds an 8×8 tiled diagonal-line (///) pattern for rendering barrier cells.
function createBarrierHatchPattern(): CanvasPattern | null {
    if (!ctx) return null;
    const size = 8;
    const pc = new OffscreenCanvas(size, size);
    const px = pc.getContext("2d");
    if (!px) return null;
    px.strokeStyle = challengeBorderColor;
    px.lineWidth = 1;
    px.beginPath();
    for (let i = -size; i <= size * 2; i += 4) {
        px.moveTo(i, 0);
        px.lineTo(i + size, size);
    }
    px.stroke();
    return ctx.createPattern(pc, "repeat");
}

// Draws all cached barrier cells using the diagonal-line hatch pattern.
function drawBarriers(): void {
    if (
        !ctx ||
        !layout ||
        !barrierHatchPattern ||
        !barrierCells ||
        barrierCells.length === 0
    )
        return;
    const { gridX, gridY, gridW, gridH, gridCellsX, gridCellsY } = layout;
    const cellW = gridW / gridCellsX;
    const cellH = gridH / gridCellsY;
    ctx.fillStyle = barrierHatchPattern;
    for (let i = 0; i < barrierCells.length; i += 2) {
        const gx = barrierCells[i];
        const gy = barrierCells[i + 1];
        ctx.fillRect(gridX + gx * cellW, gridY + gy * cellH, cellW, cellH);
    }
}

// ── Challenges ────────────────────────────────────────────────────────────────

// Maps ChallengeSpec.kind strings to biosim_challenge_kind_t ordinals (challenge_defs.h).
const CHALLENGE_KIND_INT: Record<string, number> = {
    x_band: 0,
    disc: 1,
    corners: 2,
    neighbor_count: 3,
    center_sparse: 4,
    against_wall: 5,
    migrate_distance: 6,
    touch_any_wall: 7,
    radioactive_walls: 8,
    pairs: 9,
    location_sequence: 10,
    near_barrier: 11,
    altruism: 12,
};

// Current challenge spec; updated on every configure command.
// Default matches the hardcoded s_challenge initialiser in bindings.c.
let currentChallenge: ChallengeSpec = {
    kind: "x_band",
    xMin: 0.5,
    xMax: 1.0,
    mirror: false,
};

// Overlay colours for challenge target zones. Initialised from CSS tokens passed
// via the "canvas" command; fallbacks used in tests.
let challengeOverlayColor = "rgba(0, 0, 0, 0.12)";
let challengeBorderColor = "rgba(0, 0, 0, 0.35)";
// Tiled dot pattern for challenge near-edge strips; created once after ctx is ready.
let hatchPattern: CanvasPattern | null = null;

function setChallengeSpec(spec: ChallengeSpec): void {
    biosim!.ccall(
        "biosim_wasm_set_challenge_kind",
        null,
        ["number"],
        [CHALLENGE_KIND_INT[spec.kind]],
    );
    switch (spec.kind) {
        case "x_band":
            biosim!.ccall(
                "biosim_wasm_set_challenge_x_band",
                null,
                ["number", "number", "number"],
                [spec.xMin, spec.xMax, spec.mirror ? 1 : 0],
            );
            break;
        case "disc":
            biosim!.ccall(
                "biosim_wasm_set_challenge_disc",
                null,
                ["number", "number", "number", "number"],
                [spec.x, spec.y, spec.radius, spec.weighted ? 1 : 0],
            );
            break;
        case "corners":
            biosim!.ccall(
                "biosim_wasm_set_challenge_corners",
                null,
                ["number", "number"],
                [spec.radius, spec.weighted ? 1 : 0],
            );
            break;
        case "neighbor_count":
            biosim!.ccall(
                "biosim_wasm_set_challenge_neighbor_count",
                null,
                ["number", "number", "number", "number"],
                [spec.radius, spec.minN, spec.maxN, spec.excludeBorder ? 1 : 0],
            );
            break;
        case "center_sparse":
            biosim!.ccall(
                "biosim_wasm_set_challenge_center_sparse",
                null,
                [
                    "number",
                    "number",
                    "number",
                    "number",
                    "number",
                    "number",
                    "number",
                ],
                [
                    spec.x,
                    spec.y,
                    spec.outerR,
                    spec.innerR,
                    spec.minN,
                    spec.maxN,
                    spec.weighted ? 1 : 0,
                ],
            );
            break;
        case "near_barrier":
            biosim!.ccall(
                "biosim_wasm_set_challenge_near_barrier",
                null,
                ["number"],
                [spec.radius],
            );
            break;
        case "location_sequence":
            biosim!.ccall(
                "biosim_wasm_set_challenge_location_sequence",
                null,
                ["number"],
                [spec.radius],
            );
            break;
        default:
            // No-parameter kinds (against_wall, migrate_distance, touch_any_wall,
            // radioactive_walls, pairs, altruism): kind already set above.
            break;
    }
}

// Builds an 8×8 tiled dot pattern on a scratch OffscreenCanvas.
function createHatchPattern(): CanvasPattern | null {
    if (!ctx) return null;
    const size = 8;
    const pc = new OffscreenCanvas(size, size);
    const px = pc.getContext("2d");
    if (!px) return null;
    px.fillStyle = challengeOverlayColor;
    px.beginPath();
    px.arc(size / 2, size / 2, 1.5, 0, Math.PI * 2);
    px.fill();
    return ctx.createPattern(pc, "repeat");
}

function drawChallengeOverlay(spec: ChallengeSpec): void {
    // hatchPattern is always non-null when ctx is non-null (both set together in
    // the "canvas" message handler), so a single combined guard is sufficient.
    if (!ctx || !layout || !hatchPattern) return;
    const pattern = hatchPattern;

    const { gridX, gridY, gridW, gridH, gridCellsX, gridCellsY } = layout;
    const cellW = gridW / gridCellsX;
    const cellH = gridH / gridCellsY;
    // Strip width: two cells wide, at least 4px.
    const stripW = Math.max(4, cellW * 2);
    const stripH = Math.max(4, cellH * 2);
    const stripR = Math.max(4, Math.min(cellW, cellH) * 2);

    ctx.save();
    ctx.strokeStyle = challengeBorderColor;
    ctx.lineWidth = 1.5;

    switch (spec.kind) {
        case "x_band": {
            const x0 = gridX + spec.xMin * gridW;
            const x1 = gridX + spec.xMax * gridW;
            // Border: vertical lines at the safe zone boundaries.
            ctx.beginPath();
            ctx.moveTo(x0, gridY);
            ctx.lineTo(x0, gridY + gridH);
            ctx.moveTo(x1, gridY);
            ctx.lineTo(x1, gridY + gridH);
            ctx.stroke();
            ctx.fillStyle = pattern;
            if (!spec.mirror) {
                // Safe zone is the band between xMin and xMax.
                // Near-edge strips inside the band, clamped to band width.
                const lw = Math.min(stripW, x1 - x0);
                ctx.fillRect(x0, gridY, lw, gridH);
                const rx = Math.max(x0, x1 - stripW);
                ctx.fillRect(rx, gridY, x1 - rx, gridH);
            } else {
                // Safe zones are the two outer bands.
                // Near-edge strips at the inner boundary of each outer band.
                const lx = Math.max(gridX, x0 - stripW);
                ctx.fillRect(lx, gridY, x0 - lx, gridH);
                const rx2 = Math.min(gridX + gridW, x1 + stripW);
                ctx.fillRect(x1, gridY, rx2 - x1, gridH);
            }
            break;
        }
        case "disc": {
            const cx = gridX + spec.x * gridW;
            const cy = gridY + spec.y * gridH;
            const r = spec.radius * Math.min(gridW, gridH);
            ctx.beginPath();
            ctx.arc(cx, cy, r, 0, Math.PI * 2);
            ctx.stroke();
            if (r > stripR) {
                ctx.save();
                ctx.beginPath();
                ctx.arc(cx, cy, r, 0, Math.PI * 2, false);
                ctx.arc(cx, cy, r - stripR, 0, Math.PI * 2, false);
                ctx.clip("evenodd");
                ctx.fillStyle = pattern;
                ctx.fillRect(cx - r, cy - r, r * 2, r * 2);
                ctx.restore();
            }
            break;
        }
        case "corners": {
            const r = spec.radius * Math.min(gridW, gridH);
            for (const [fx, fy] of [
                [0, 0],
                [1, 0],
                [0, 1],
                [1, 1],
            ] as [number, number][]) {
                const cx = gridX + fx * gridW;
                const cy = gridY + fy * gridH;
                ctx.save();
                // Clip to the grid so arcs do not bleed outside the grid region.
                ctx.beginPath();
                ctx.rect(gridX, gridY, gridW, gridH);
                ctx.clip();
                ctx.beginPath();
                ctx.arc(cx, cy, r, 0, Math.PI * 2);
                ctx.stroke();
                if (r > stripR) {
                    ctx.save();
                    ctx.beginPath();
                    ctx.arc(cx, cy, r, 0, Math.PI * 2, false);
                    ctx.arc(cx, cy, r - stripR, 0, Math.PI * 2, false);
                    ctx.clip("evenodd");
                    ctx.fillStyle = pattern;
                    ctx.fillRect(cx - r, cy - r, r * 2, r * 2);
                    ctx.restore();
                }
                ctx.restore();
            }
            break;
        }
        case "against_wall":
        case "radioactive_walls": {
            const bW = Math.max(2, cellW * 2);
            const bH = Math.max(2, cellH * 2);
            // Inner boundary lines of each wall band.
            ctx.beginPath();
            ctx.moveTo(gridX, gridY + bH);
            ctx.lineTo(gridX + gridW, gridY + bH);
            ctx.moveTo(gridX, gridY + gridH - bH);
            ctx.lineTo(gridX + gridW, gridY + gridH - bH);
            ctx.moveTo(gridX + bW, gridY);
            ctx.lineTo(gridX + bW, gridY + gridH);
            ctx.moveTo(gridX + gridW - bW, gridY);
            ctx.lineTo(gridX + gridW - bW, gridY + gridH);
            ctx.stroke();
            // Near-edge strips inside each band, near the inner boundary.
            ctx.fillStyle = pattern;
            ctx.fillRect(gridX, gridY + bH - stripH, gridW, stripH);
            ctx.fillRect(gridX, gridY + gridH - bH, gridW, stripH);
            ctx.fillRect(gridX + bW - stripW, gridY, stripW, gridH);
            ctx.fillRect(gridX + gridW - bW, gridY, stripW, gridH);
            break;
        }
        case "center_sparse": {
            const cx = gridX + spec.x * gridW;
            const cy = gridY + spec.y * gridH;
            const s = Math.min(gridW, gridH);
            const outerR = spec.outerR * s;
            const innerR = spec.innerR * s;
            ctx.beginPath();
            ctx.arc(cx, cy, outerR, 0, Math.PI * 2);
            ctx.stroke();
            ctx.beginPath();
            ctx.arc(cx, cy, innerR, 0, Math.PI * 2);
            ctx.stroke();
            if (outerR > stripR) {
                ctx.save();
                ctx.beginPath();
                ctx.arc(cx, cy, outerR, 0, Math.PI * 2, false);
                ctx.arc(cx, cy, outerR - stripR, 0, Math.PI * 2, false);
                ctx.clip("evenodd");
                ctx.fillStyle = pattern;
                ctx.fillRect(cx - outerR, cy - outerR, outerR * 2, outerR * 2);
                ctx.restore();
            }
            if (innerR + stripR < outerR) {
                ctx.save();
                ctx.beginPath();
                const sr = innerR + stripR;
                ctx.arc(cx, cy, sr, 0, Math.PI * 2, false);
                ctx.arc(cx, cy, innerR, 0, Math.PI * 2, false);
                ctx.clip("evenodd");
                ctx.fillStyle = pattern;
                ctx.fillRect(cx - sr, cy - sr, sr * 2, sr * 2);
                ctx.restore();
            }
            break;
        }
        default:
            // No geometric overlay for: migrate_distance, touch_any_wall,
            // neighbor_count, pairs, altruism, near_barrier, location_sequence.
            break;
    }

    ctx.restore();
}

// ── Agents ────────────────────────────────────────────────────────────────────

let selectedAgentId: number | null = null;
let hoveredAgentId: number | null = null;

function readAgentInfo(id: number): AgentInfo {
    const { HEAP32, HEAPU8, HEAPU32 } = biosim!;
    const HEAPU16 = new Uint16Array(HEAPU8.buffer);
    const HEAPF32 = new Float32Array(HEAPU8.buffer);
    const aliveOff = call("biosim_wasm_get_alive_ptr");
    const locXOff = call("biosim_wasm_get_loc_x_ptr") >>> 2;
    const locYOff = call("biosim_wasm_get_loc_y_ptr") >>> 2;
    const bxOff = call("biosim_wasm_get_birth_x_ptr") >>> 2;
    const byOff = call("biosim_wasm_get_birth_y_ptr") >>> 2;
    const dirOff = call("biosim_wasm_get_last_move_dir_ptr");
    const oscOff = call("biosim_wasm_get_osc_period_ptr") >>> 1;
    const respOff = call("biosim_wasm_get_responsiveness_ptr") >>> 2;
    const losOff = call("biosim_wasm_get_los_range_ptr");
    const cbOff = call("biosim_wasm_get_challenge_bits_ptr") >>> 2;
    const fpOff = call("biosim_wasm_get_genome_fingerprint_ptr") >>> 2;
    const lo = HEAPU32[fpOff + id * 2];
    const hi = HEAPU32[fpOff + id * 2 + 1];
    return {
        id,
        alive: HEAPU8[aliveOff + id] !== 0,
        gx: HEAP32[locXOff + id],
        gy: HEAP32[locYOff + id],
        birthX: HEAP32[bxOff + id],
        birthY: HEAP32[byOff + id],
        heading: HEAPU8[dirOff + id] & 7,
        oscPeriod: HEAPU16[oscOff + id],
        responsiveness: HEAPF32[respOff + id],
        losRange: HEAPU8[losOff + id],
        challengeBits: HEAPU32[cbOff + id],
        fingerprint:
            hi.toString(16).padStart(8, "0") + lo.toString(16).padStart(8, "0"),
    };
}

function notifySelectionUpdate(): void {
    if (selectedAgentId === null || !biosim) return;
    postMessage({
        type: "agentUpdated",
        info: readAgentInfo(selectedAgentId),
    } satisfies WorkerEvent);
}

// Reads and decodes agent `id`'s neural network from the WASM heap. The conn /
// weight arrays are column-major with a `slot * pop + id` stride (nnet.h).
function readBrain(id: number): {
    conns: BrainConn[];
    neuronCount: number;
} {
    const { HEAPU8 } = biosim!;
    const HEAPU16 = new Uint16Array(HEAPU8.buffer);
    const HEAPI16 = new Int16Array(HEAPU8.buffer);
    const pop = call("biosim_wasm_get_population");
    // genome_conn/conn_length are uint16 (>>1), genome_wgt is int16 (>>1),
    // neuron_count is uint8 (no shift) — mirror the typed-view discipline.
    const connOff = call("biosim_wasm_get_genome_conn_ptr") >>> 1;
    const wgtOff = call("biosim_wasm_get_genome_wgt_ptr") >>> 1;
    const lenOff = call("biosim_wasm_get_conn_length_ptr") >>> 1;
    const neuronOff = call("biosim_wasm_get_neuron_count_ptr");
    const connLen = HEAPU16[lenOff + id];
    const conns: BrainConn[] = [];
    for (let slot = 0; slot < connLen; slot++) {
        const i = slot * pop + id;
        conns.push(unpackConn(HEAPU16[connOff + i], HEAPI16[wgtOff + i]));
    }
    return { conns, neuronCount: HEAPU8[neuronOff + id] };
}

// ── Rendering ─────────────────────────────────────────────────────────────────

// Shared fill style for agents in all modes.
// Matches --color-text (#0a0a0a). The worker cannot read CSS custom properties;
// update this literal if the palette changes.
// The selected-cell accent colour is applied by the UI overlay, not the canvas.
const AGENT_COLOR = "#0a0a0a";

let ctx: OffscreenCanvasRenderingContext2D | null = null;
// Matches --_accent; updated from the main thread on canvas init.
let accentColor = "#15803d";
let layout: Layout | null = null;

let mode: Mode = "idle";
let startTime = performance.now(); // epoch for kinematic t (seconds)
let kFrozenT = 0; // kinematic t captured at the moment play/step was pressed
let transitionStart = 0; // performance.now() when current transition began
const TRANSITION_IN_MS = 600;
let animInterval: ReturnType<typeof setInterval> | null = null;

function clearCanvas(): void {
    if (!ctx || !layout) return;
    // Transparent clear — CSS dot-grid and GridView overlay show through.
    ctx.clearRect(0, 0, layout.canvasW, layout.canvasH);
}

function applyAgentStyle(): void {
    if (!ctx) return;
    ctx.fillStyle = AGENT_COLOR;
}

function drawKinematic(t: number): void {
    if (!ctx || !layout || !biosim) return;
    clearCanvas();

    const { canvasW, canvasH } = layout;
    const pop = call("biosim_wasm_get_population");

    applyAgentStyle();
    ctx.beginPath();
    for (let i = 0; i < pop; i++) {
        const { x, y, r } = kinematicPosition(i, pop, {
            t,
            canvasW,
            canvasH,
            beat: 0,
            pointer: null,
        });
        ctx.moveTo(x + r, y);
        ctx.arc(x, y, r, 0, Math.PI * 2);
    }
    ctx.fill();
}

function drawGrid(): void {
    if (!ctx || !layout || !biosim) return;
    clearCanvas();
    drawChallengeOverlay(currentChallenge);
    drawBarriers();

    const { gridX, gridY, gridW, gridH, gridCellsX, gridCellsY } = layout;
    const pop = call("biosim_wasm_get_population");
    const locXOff = call("biosim_wasm_get_loc_x_ptr") >>> 2;
    const locYOff = call("biosim_wasm_get_loc_y_ptr") >>> 2;
    const aliveOff = call("biosim_wasm_get_alive_ptr");
    const { HEAP32, HEAPU8 } = biosim;

    applyAgentStyle();
    ctx.beginPath();
    for (let i = 0; i < pop; i++) {
        if (HEAPU8[aliveOff + i]) {
            const gx = HEAP32[locXOff + i];
            const gy = HEAP32[locYOff + i];
            const { x, y, r } = gridPosition(
                gx,
                gy,
                gridX,
                gridY,
                gridW,
                gridH,
                gridCellsX,
                gridCellsY,
            );
            ctx.moveTo(x + r, y);
            ctx.arc(x, y, r, 0, Math.PI * 2);
        }
    }
    ctx.fill();

    ctx.fillStyle = accentColor;
    for (const id of [hoveredAgentId, selectedAgentId]) {
        if (id !== null && HEAPU8[aliveOff + id]) {
            const gx = HEAP32[locXOff + id];
            const gy = HEAP32[locYOff + id];
            const { x, y, r } = gridPosition(
                gx,
                gy,
                gridX,
                gridY,
                gridW,
                gridH,
                gridCellsX,
                gridCellsY,
            );
            ctx.beginPath();
            ctx.arc(x, y, r, 0, Math.PI * 2);
            ctx.fill();
        }
    }

    if (selectedAgentId !== null && HEAPU8[aliveOff + selectedAgentId]) {
        const gx = HEAP32[locXOff + selectedAgentId];
        const gy = HEAP32[locYOff + selectedAgentId];
        const { x, y, r } = gridPosition(
            gx,
            gy,
            gridX,
            gridY,
            gridW,
            gridH,
            gridCellsX,
            gridCellsY,
        );
        ctx.beginPath();
        ctx.arc(x, y, r + 3, 0, Math.PI * 2);
        ctx.strokeStyle = AGENT_COLOR;
        ctx.lineWidth = 1;
        ctx.stroke();
    }
}

function drawTransitionIn(frac: number): void {
    if (!ctx || !layout || !biosim) return;
    clearCanvas();
    drawChallengeOverlay(currentChallenge);
    drawBarriers();

    const {
        canvasW,
        canvasH,
        gridX,
        gridY,
        gridW,
        gridH,
        gridCellsX,
        gridCellsY,
    } = layout;
    const pop = call("biosim_wasm_get_population");
    const locXOff = call("biosim_wasm_get_loc_x_ptr") >>> 2;
    const locYOff = call("biosim_wasm_get_loc_y_ptr") >>> 2;
    const aliveOff = call("biosim_wasm_get_alive_ptr");
    const { HEAP32, HEAPU8 } = biosim;

    applyAgentStyle();
    ctx.beginPath();
    for (let i = 0; i < pop; i++) {
        const from = kinematicPosition(i, pop, {
            t: kFrozenT,
            canvasW,
            canvasH,
            beat: 0,
            pointer: null,
        });
        if (HEAPU8[aliveOff + i]) {
            const gx = HEAP32[locXOff + i];
            const gy = HEAP32[locYOff + i];
            const to = gridPosition(
                gx,
                gy,
                gridX,
                gridY,
                gridW,
                gridH,
                gridCellsX,
                gridCellsY,
            );
            const { x, y } = lerpVec2(from, to, frac);
            const r = from.r + (to.r - from.r) * frac;
            ctx.moveTo(x + r, y);
            ctx.arc(x, y, r, 0, Math.PI * 2);
        } else {
            // Dead agents fade out at their kinematic position.
            const r = from.r * (1 - frac);
            if (r > 0.1) {
                ctx.moveTo(from.x + r, from.y);
                ctx.arc(from.x, from.y, r, 0, Math.PI * 2);
            }
        }
    }
    ctx.fill();
}

function startTransitionIfNeeded(): void {
    if (mode === "idle") {
        kFrozenT = (performance.now() - startTime) / 1000;
        transitionStart = performance.now();
        mode = "transitioning-in";
    }
}

function animTick(): void {
    if (!ctx || !layout || !biosim) return;
    if (freeRunning) return;
    const now = performance.now();
    const t = (now - startTime) / 1000;

    if (mode === "transitioning-in") {
        const raw = (now - transitionStart) / TRANSITION_IN_MS;
        const frac = Math.min(1, raw);
        drawTransitionIn(easeInOut(frac));
        if (frac >= 1) mode = "running";
    } else if (mode === "running") {
        drawGrid();
    } else {
        drawKinematic(t);
    }
}

function startAnimLoop(): void {
    if (animInterval !== null) return;
    animInterval = setInterval(animTick, 1000 / 60);
}

// ── Simulation control ────────────────────────────────────────────────────────

let playing = false;
let freeRunning = false;
let targetFps = 0;
let fpsWindow = createFpsWindow();

function applyConfig(p: SimParams): void {
    setParamInt("population", p.population);
    setParamInt("grid-size-x", p.gridSizeX);
    setParamInt("grid-size-y", p.gridSizeY);
    setParamInt("steps-per-gen", p.stepsPerGen);
    setParamInt("max-genes", p.maxGenes);
    setParamInt("max-neurons", p.maxNeurons);
    setParamFloat("point-mutation-rate", p.pointMutationRate);
    setParamBool("sexual-reproduction", p.sexualReproduction);
    setParamBool("choose-parents-by-fitness", p.chooseParentsByFitness);
    setParamInt("los-range", p.losRange);
    setParamInt("sensor-radius", p.sensorRadius);
    setParamBool("enable-kill", p.enableKill);
    setParamFloat("responsiveness-curve-k", p.responsivenessCurveK);
    setBarriers(p.barriers);
    setChallengeSpec(p.challenge);
    currentChallenge = p.challenge;
}

function progress(type: "stepped" | "genComplete" | "paused"): WorkerEvent {
    return {
        type,
        gen: call("biosim_wasm_get_gen"),
        step: call("biosim_wasm_get_step"),
    };
}

function handlePlay(): void {
    startTransitionIfNeeded();
    fpsWindow = createFpsWindow();
    playing = true;
    postMessage(progress("stepped"));
    playTick();
}

function handleStop(): void {
    playing = false;
    // mode intentionally unchanged — agents freeze at their current
    // grid positions (idle-timeout return to kinematic is future work).
    postMessage(progress("paused"));
}

function handleStep(): void {
    startTransitionIfNeeded();
    if (callChecked("biosim_wasm_do_step") !== 0) return;
    postMessage(progress("stepped"));
    notifySelectionUpdate();
}

function handleRewind(): void {
    playing = false;
    if (callChecked("biosim_wasm_rewind") !== 0) return;
    postMessage(progress("paused"));
}

function handleRewindConfigured(params: SimParams): void {
    playing = false;
    const prevMode = mode;
    applyConfig(params);
    if (callChecked("biosim_wasm_rewind_configured") !== 0) return;
    cacheBarrierCells();
    mode = prevMode === "idle" ? "idle" : "running";
    startTime = performance.now();
    postMessage({
        type: "rewindConfigured",
        gen: call("biosim_wasm_get_gen"),
        population: call("biosim_wasm_get_population"),
        gridSizeX: params.gridSizeX,
        gridSizeY: params.gridSizeY,
        stepsPerGen: params.stepsPerGen,
    } satisfies WorkerEvent);
}

function handleNextGenerationConfigured(params: SimParams): void {
    const prevMode = mode;
    startTransitionIfNeeded();
    applyConfig(params);
    if (callChecked("biosim_wasm_next_generation_configured") !== 0) return;
    cacheBarrierCells();
    mode = prevMode === "idle" ? "idle" : "running";
    startTime = performance.now();
    postMessage({
        type: "nextGenerationConfigured",
        gen: call("biosim_wasm_census_gen"),
        population: call("biosim_wasm_get_population"),
        censusPopulation: call("biosim_wasm_census_population"),
        survivors: call("biosim_wasm_census_survivors"),
        kills: call("biosim_wasm_census_kills"),
        requiredGenomeLen: call("biosim_wasm_get_max_genes"),
        requiredNeurons: call("biosim_wasm_get_max_neurons"),
        gridSizeX: params.gridSizeX,
        gridSizeY: params.gridSizeY,
        stepsPerGen: params.stepsPerGen,
    } satisfies WorkerEvent);
}

function handleConfigure(params: SimParams): void {
    playing = false;
    const prevMode = mode;
    applyConfig(params);
    if (callChecked("biosim_wasm_init") !== 0) return;
    cacheBarrierCells();
    // Skip the kinematic intro if the user was already watching the grid.
    mode = prevMode === "idle" ? "idle" : "running";
    startTime = performance.now();
    postMessage({
        type: "configured",
        population: params.population,
        gridSizeX: params.gridSizeX,
        gridSizeY: params.gridSizeY,
        stepsPerGen: params.stepsPerGen,
    } satisfies WorkerEvent);
}

function handleClearGenom(): void {
    playing = false;
    if (callChecked("biosim_wasm_clear_genome") !== 0) return;
    postMessage(progress("paused"));
}

function handleExportSnapshot(): void {
    if (!biosim) return;
    const rc = call("biosim_wasm_snapshot_export");
    if (rc !== 0) {
        console.error(
            `WASM call biosim_wasm_snapshot_export returned status ${rc}`,
        );
        postMessage({
            type: "error",
            message: "Snapshot export failed",
            call: "biosim_wasm_snapshot_export",
            fatal: false,
        } satisfies WorkerEvent);
        return;
    }
    const ptr = call("biosim_wasm_snapshot_export_ptr");
    const size = call("biosim_wasm_snapshot_export_size");
    const data = biosim.HEAPU8.slice(ptr, ptr + size);
    postMessage({ type: "snapshotData", data } satisfies WorkerEvent, [
        data.buffer,
    ]);
}

// Stage a dropped snapshot. This is spawn 1 + load only — it never breeds from
// the dropped survivors. `rewindFirst` (set when the drop interrupted a
// mid-generation run) rewinds the *live* snap first so the grid shows a clean
// generation-start population (spawn 1) before import overwrites `snap`. The
// dropped file is then loaded into `snap`; the machine decides whether to breed
// from it (spawn 2) based on snapshot/config compatibility, so this never
// rewinds the dropped survivors itself. Affects only the population — never the
// config.
function handleLoadSnapshot(data: Uint8Array, rewindFirst: boolean): void {
    playing = false;
    const prevMode = mode;
    if (rewindFirst) {
        call("biosim_wasm_rewind"); // spawn 1: rewind the live snap
    }
    const ptr = biosim!.ccall(
        "biosim_wasm_snapshot_import_alloc",
        "number",
        ["number"],
        [data.byteLength],
    );
    if (ptr === 0) {
        console.error(
            "WASM call biosim_wasm_snapshot_import_alloc returned null",
        );
        postMessage({
            type: "error",
            message: "Snapshot load failed (alloc)",
            call: "biosim_wasm_snapshot_import_alloc",
            fatal: false,
        } satisfies WorkerEvent);
        return;
    }
    biosim!.HEAPU8.set(data, ptr);
    const importRc = call("biosim_wasm_snapshot_import");
    if (importRc !== 0) {
        console.error(
            `WASM call biosim_wasm_snapshot_import returned status ${importRc}`,
        );
        postMessage({
            type: "error",
            message: "Snapshot load failed (import)",
            call: "biosim_wasm_snapshot_import",
            fatal: false,
        } satisfies WorkerEvent);
        return;
    }
    mode = prevMode === "idle" ? "idle" : "running";
    startTime = performance.now();
    postMessage({
        type: "snapshotLoaded",
        gen: call("biosim_wasm_get_gen"),
        population: call("biosim_wasm_get_population"),
        requiredGenomeLen: call("biosim_wasm_snapshot_max_genes"),
        requiredNeurons: call("biosim_wasm_snapshot_max_neurons"),
    } satisfies WorkerEvent);
}

function handleStartFreeRun(): void {
    playing = false;
    if (mode === "idle" || mode === "transitioning-in") {
        mode = "running";
    }
    freeRunning = true;
    clearCanvas();
    freeRunTick();
}

function handleStopFreeRun(): void {
    freeRunning = false;
    postMessage(progress("paused"));
}

// ── Handler helpers ───────────────────────────────────────────────────────────

function playTick(): void {
    if (!playing) return;
    if (call("biosim_wasm_is_gen_complete")) {
        playing = false;
        postMessage(progress("genComplete"));
        return;
    }
    const tickStart = performance.now();
    if (callChecked("biosim_wasm_do_step") !== 0) {
        playing = false;
        return;
    }
    postMessage(progress("stepped"));
    notifySelectionUpdate();
    const elapsed = performance.now() - tickStart;
    const fpsReading = fpsWindow.tick(performance.now());
    if (fpsReading !== null) {
        postMessage({ type: "fps", value: fpsReading } satisfies WorkerEvent);
    }
    if (call("biosim_wasm_is_gen_complete")) {
        playing = false;
        postMessage(progress("genComplete"));
    } else {
        setTimeout(playTick, stepDelay(targetFps, elapsed));
    }
}

function doNextGeneration(): void {
    startTransitionIfNeeded();
    if (callChecked("biosim_wasm_next_generation") !== 0) return;
    const gen = call("biosim_wasm_census_gen");
    const population = call("biosim_wasm_census_population");
    const survivors = call("biosim_wasm_census_survivors");
    const kills = call("biosim_wasm_census_kills");
    const requiredGenomeLen = call("biosim_wasm_get_max_genes");
    const requiredNeurons = call("biosim_wasm_get_max_neurons");
    postMessage({
        type: "census",
        gen,
        population,
        survivors,
        kills,
        requiredGenomeLen,
        requiredNeurons,
    } satisfies WorkerEvent);
}

function freeRunTick(): void {
    if (!freeRunning) return;
    if (callChecked("biosim_wasm_do_gen") !== 0) {
        freeRunning = false;
        return;
    }
    const gen = call("biosim_wasm_census_gen");
    const population = call("biosim_wasm_census_population");
    const survivors = call("biosim_wasm_census_survivors");
    const kills = call("biosim_wasm_census_kills");
    const requiredGenomeLen = call("biosim_wasm_get_max_genes");
    const requiredNeurons = call("biosim_wasm_get_max_neurons");
    postMessage({
        type: "census",
        gen,
        population,
        survivors,
        kills,
        requiredGenomeLen,
        requiredNeurons,
    } satisfies WorkerEvent);
    setTimeout(freeRunTick, 0);
}

// ── Message handler ───────────────────────────────────────────────────────────

self.addEventListener("message", (e: MessageEvent<WorkerCmd>) => {
    const cmd = e.data;
    switch (cmd.type) {
        case "reset":
            playing = false;
            mode = "idle";
            startTime = performance.now();
            break;
        case "configure":
            handleConfigure(cmd.params);
            break;
        case "play":
            handlePlay();
            break;
        case "stop":
            handleStop();
            break;
        case "step":
            handleStep();
            break;
        case "rewind":
            handleRewind();
            break;
        case "rewindConfigured":
            handleRewindConfigured(cmd.params);
            break;
        case "nextGeneration":
            doNextGeneration();
            break;
        case "nextGenerationConfigured":
            handleNextGenerationConfigured(cmd.params);
            break;
        case "clearGenom":
            handleClearGenom();
            break;
        case "startFreeRun":
            handleStartFreeRun();
            break;
        case "stopFreeRun":
            handleStopFreeRun();
            break;
        case "exportSnapshot":
            handleExportSnapshot();
            break;
        case "loadSnapshot":
            handleLoadSnapshot(cmd.data, cmd.rewindFirst);
            break;
        // Pick commands only *query* the heap and reply with agentPicked/
        // agentMissed — they never mutate selection. The main thread routes the
        // reply through AgentFocus and echoes back selectAgent/hoverAgent (the
        // sole writers of selectedAgentId/hoveredAgentId; see App.svelte).
        case "pickAgentAtCell": {
            if (!biosim || !layout) break;
            const W = layout.gridCellsX;
            const cellsOff = call("biosim_wasm_get_grid_cells_ptr") >>> 2;
            const raw = biosim.HEAPU32[cellsOff + cmd.gy * W + cmd.gx];
            if (raw === 0 || raw === 0xffffffff) {
                postMessage({
                    type: "agentMissed",
                    reason: cmd.reason,
                } satisfies WorkerEvent);
            } else {
                postMessage({
                    type: "agentPicked",
                    reason: cmd.reason,
                    info: readAgentInfo(raw - 1),
                } satisfies WorkerEvent);
            }
            break;
        }
        case "navigateAgent": {
            if (!biosim) break;
            const pop = call("biosim_wasm_get_population");
            const id = (((cmd.fromId + cmd.direction) % pop) + pop) % pop;
            postMessage({
                type: "agentPicked",
                reason: "click",
                info: readAgentInfo(id),
            } satisfies WorkerEvent);
            break;
        }
        case "randomAgent": {
            if (!biosim) break;
            const pop = call("biosim_wasm_get_population");
            const aliveOff = call("biosim_wasm_get_alive_ptr");
            const alive: number[] = [];
            for (let i = 0; i < pop; i++) {
                if (biosim.HEAPU8[aliveOff + i]) alive.push(i);
            }
            if (alive.length === 0) break;
            const id = alive[Math.floor(Math.random() * alive.length)];
            postMessage({
                type: "agentPicked",
                reason: "click",
                info: readAgentInfo(id),
            } satisfies WorkerEvent);
            break;
        }
        case "selectAgentById": {
            if (!biosim) break;
            const pop = call("biosim_wasm_get_population");
            const id = ((cmd.id % pop) + pop) % pop;
            postMessage({
                type: "agentPicked",
                reason: "click",
                info: readAgentInfo(id),
            } satisfies WorkerEvent);
            break;
        }
        case "requestBrain": {
            if (!biosim) break;
            const { conns, neuronCount } = readBrain(cmd.id);
            postMessage({
                type: "brainData",
                id: cmd.id,
                conns,
                neuronCount,
            } satisfies WorkerEvent);
            break;
        }
        case "setSpeed":
            targetFps = cmd.fps;
            break;
        // Sole writers of the worker's selection state (set by AgentFocus on the
        // main thread — pick replies and UX gestures both funnel through here).
        case "selectAgent":
            selectedAgentId = cmd.id;
            break;
        case "hoverAgent":
            hoveredAgentId = cmd.id;
            break;
        case "canvas": {
            const offscreen = cmd.canvas;
            challengeOverlayColor = cmd.overlayColor;
            challengeBorderColor = cmd.borderColor;
            accentColor = cmd.accentColor;
            if (layout) {
                offscreen.width = layout.canvasW;
                offscreen.height = layout.canvasH;
            } else if (biosim) {
                offscreen.width = call("biosim_wasm_get_size_x") * 4;
                offscreen.height = call("biosim_wasm_get_size_y") * 4;
            }
            ctx = offscreen.getContext("2d");
            hatchPattern = createHatchPattern();
            barrierHatchPattern = createBarrierHatchPattern();
            startAnimLoop();
            break;
        }
        case "layout": {
            // Grid cell counts come straight from the WASM sim (the source of
            // truth, set by applyConfig), not from the command — the UI no
            // longer round-trips grid dimensions back to the worker.
            layout = {
                canvasW: cmd.canvasW,
                canvasH: cmd.canvasH,
                gridX: cmd.gridX,
                gridY: cmd.gridY,
                gridW: cmd.gridW,
                gridH: cmd.gridH,
                gridCellsX: call("biosim_wasm_get_size_x"),
                gridCellsY: call("biosim_wasm_get_size_y"),
            };
            // Resizing the OffscreenCanvas clears its contents; animTick will
            // redraw on the next frame.
            if (ctx) {
                ctx.canvas.width = cmd.canvasW;
                ctx.canvas.height = cmd.canvasH;
            }
            break;
        }
    }
});

// ── Initialisation ────────────────────────────────────────────────────────────

async function init(): Promise<void> {
    const mod = await import(/* @vite-ignore */ biosimUrl);
    const createBiosim = mod.default as EmscriptenFactory;
    biosim = await createBiosim({
        locateFile: (filename: string) =>
            `${import.meta.env.BASE_URL}wasm/${filename}`,
    });
    if (callChecked("biosim_wasm_init", true) !== 0) return;
    postMessage({ type: "ready" } satisfies WorkerEvent);
}

init();
