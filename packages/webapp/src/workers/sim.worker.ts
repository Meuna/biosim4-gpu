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

/** Scalar parameters that can be configured before (re-)initialising the sim. */
export interface SimParams {
    population: number;
    gridSizeX: number;
    gridSizeY: number;
    stepsPerGen: number;
    maxGenomeLen: number;
    maxNeurons: number;
    pointMutationRate: number;
    sexualReproduction: boolean;
    chooseParentsByFitness: boolean;
    losRange: number;
    sensorRadius: number;
    enableKill: boolean;
    responsivenessCurveK: number;
    challenge: ChallengeSpec;
}

export type WorkerCmd =
    | { type: "play" }
    | { type: "stop" }
    | { type: "reset" }
    | { type: "step" }
    | { type: "stepAgent" }
    | { type: "nextGeneration" }
    | { type: "configure"; params: SimParams }
    | {
          type: "canvas";
          canvas: OffscreenCanvas;
          overlayColor: string;
          borderColor: string;
      }
    | {
          type: "layout";
          canvasW: number;
          canvasH: number;
          gridX: number;
          gridY: number;
          gridW: number;
          gridH: number;
          gridCellsX: number;
          gridCellsY: number;
      };

export type WorkerEvent =
    | { type: "ready" }
    | { type: "status"; message: string }
    | {
          type: "census";
          gen: number;
          population: number;
          survivors: number;
          kills: number;
      }
    | {
          type: "configured";
          population: number;
          gridSizeX: number;
          gridSizeY: number;
          stepsPerGen: number;
      }
    | { type: "error"; message: string };

// ── Rendering mode ────────────────────────────────────────────────────────────

type Mode = "idle" | "transitioning-in" | "running";

// ── Module-level state ────────────────────────────────────────────────────────

let biosim: EmscriptenModule | null = null;
let playing = false;
let ctx: OffscreenCanvasRenderingContext2D | null = null;

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
// Tiled hatch pattern for the near-edge strip; created once after ctx is ready.
let hatchPattern: CanvasPattern | null = null;

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

let layout: Layout | null = null;
let mode: Mode = "idle";
let startTime = performance.now(); // epoch for kinematic t (seconds)
let kFrozenT = 0; // kinematic t captured at the moment play/step was pressed
let transitionStart = 0; // performance.now() when current transition began
const TRANSITION_IN_MS = 600;
let animInterval: ReturnType<typeof setInterval> | null = null;

// ── Utility ───────────────────────────────────────────────────────────────────

function call(name: string): number {
    return biosim!.ccall(name, "number", [], []);
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

// ── Rendering ─────────────────────────────────────────────────────────────────

// Shared fill style for agents in all modes.
// Matches --color-text (#0a0a0a). The worker cannot read CSS custom properties;
// update this literal if the palette changes.
// The selected-cell accent colour is applied by the UI overlay, not the canvas.
const AGENT_COLOR = "#0a0a0a";

function applyAgentStyle(): void {
    if (!ctx) return;
    ctx.fillStyle = AGENT_COLOR;
}

function clearCanvas(): void {
    if (!ctx || !layout) return;
    // Transparent clear — CSS dot-grid and GridView overlay show through.
    ctx.clearRect(0, 0, layout.canvasW, layout.canvasH);
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
    if (!ctx || !layout) return;
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
            if (hatchPattern) {
                ctx.fillStyle = hatchPattern;
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
            if (hatchPattern && r > stripR) {
                ctx.save();
                ctx.beginPath();
                ctx.arc(cx, cy, r, 0, Math.PI * 2, false);
                ctx.arc(cx, cy, r - stripR, 0, Math.PI * 2, false);
                ctx.clip("evenodd");
                ctx.fillStyle = hatchPattern;
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
                ctx.beginPath();
                ctx.arc(cx, cy, r, 0, Math.PI * 2);
                ctx.stroke();
                if (hatchPattern && r > stripR) {
                    ctx.save();
                    ctx.beginPath();
                    ctx.arc(cx, cy, r, 0, Math.PI * 2, false);
                    ctx.arc(cx, cy, r - stripR, 0, Math.PI * 2, false);
                    ctx.clip("evenodd");
                    ctx.fillStyle = hatchPattern;
                    ctx.fillRect(cx - r, cy - r, r * 2, r * 2);
                    ctx.restore();
                }
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
            if (hatchPattern) {
                ctx.fillStyle = hatchPattern;
                ctx.fillRect(gridX, gridY + bH - stripH, gridW, stripH);
                ctx.fillRect(gridX, gridY + gridH - bH, gridW, stripH);
                ctx.fillRect(gridX + bW - stripW, gridY, stripW, gridH);
                ctx.fillRect(gridX + gridW - bW, gridY, stripW, gridH);
            }
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
            if (hatchPattern) {
                if (outerR > stripR) {
                    ctx.save();
                    ctx.beginPath();
                    ctx.arc(cx, cy, outerR, 0, Math.PI * 2, false);
                    ctx.arc(cx, cy, outerR - stripR, 0, Math.PI * 2, false);
                    ctx.clip("evenodd");
                    ctx.fillStyle = hatchPattern;
                    ctx.fillRect(
                        cx - outerR,
                        cy - outerR,
                        outerR * 2,
                        outerR * 2,
                    );
                    ctx.restore();
                }
                if (innerR + stripR < outerR) {
                    ctx.save();
                    ctx.beginPath();
                    const sr = innerR + stripR;
                    ctx.arc(cx, cy, sr, 0, Math.PI * 2, false);
                    ctx.arc(cx, cy, innerR, 0, Math.PI * 2, false);
                    ctx.clip("evenodd");
                    ctx.fillStyle = hatchPattern;
                    ctx.fillRect(cx - sr, cy - sr, sr * 2, sr * 2);
                    ctx.restore();
                }
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

function drawKinematic(t: number): void {
    if (!ctx || !layout || !biosim) return;
    clearCanvas();

    const { canvasW, canvasH } = layout;
    const pop = call("biosim_wasm_get_population");
    const cx = canvasW / 2;
    const cy = canvasH / 2;

    applyAgentStyle();
    ctx.beginPath();
    for (let i = 0; i < pop; i++) {
        const { x, y, r } = kinematicPosition(
            i,
            pop,
            cx,
            cy,
            t,
            canvasW,
            canvasH,
        );
        ctx.moveTo(x + r, y);
        ctx.arc(x, y, r, 0, Math.PI * 2);
    }
    ctx.fill();
}

function drawGrid(): void {
    if (!ctx || !layout || !biosim) return;
    clearCanvas();
    drawChallengeOverlay(currentChallenge);

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
}

function drawTransitionIn(frac: number): void {
    if (!ctx || !layout || !biosim) return;
    clearCanvas();
    drawChallengeOverlay(currentChallenge);

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
    const cx = canvasW / 2;
    const cy = canvasH / 2;
    const pop = call("biosim_wasm_get_population");
    const locXOff = call("biosim_wasm_get_loc_x_ptr") >>> 2;
    const locYOff = call("biosim_wasm_get_loc_y_ptr") >>> 2;
    const aliveOff = call("biosim_wasm_get_alive_ptr");
    const { HEAP32, HEAPU8 } = biosim;

    applyAgentStyle();
    ctx.beginPath();
    for (let i = 0; i < pop; i++) {
        const from = kinematicPosition(
            i,
            pop,
            cx,
            cy,
            kFrozenT,
            canvasW,
            canvasH,
        );
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

// ── Animation loop ────────────────────────────────────────────────────────────

function animTick(): void {
    if (!ctx || !layout || !biosim) return;
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

// ── Simulation step helpers ───────────────────────────────────────────────────

function startTransitionIfNeeded(): void {
    if (mode === "idle") {
        kFrozenT = (performance.now() - startTime) / 1000;
        transitionStart = performance.now();
        mode = "transitioning-in";
    }
}

function doStep(): void {
    startTransitionIfNeeded();
    call("biosim_wasm_do_step");
    const step = call("biosim_wasm_get_step");
    postMessage({
        type: "status",
        message: `Run step ${step}`,
    } satisfies WorkerEvent);
}

function doStepAgent(): void {
    startTransitionIfNeeded();
    call("biosim_wasm_do_step_agent");
    const agent = call("biosim_wasm_get_last_agent");
    const step = call("biosim_wasm_get_step");
    postMessage({
        type: "status",
        message: `Run a agent ${agent} at step ${step}`,
    } satisfies WorkerEvent);
}

function doNextGeneration(): void {
    startTransitionIfNeeded();
    call("biosim_wasm_next_generation");
    const gen = call("biosim_wasm_census_gen");
    const population = call("biosim_wasm_census_population");
    const survivors = call("biosim_wasm_census_survivors");
    const kills = call("biosim_wasm_census_kills");
    postMessage({
        type: "census",
        gen,
        population,
        survivors,
        kills,
    } satisfies WorkerEvent);
}

function playTick(): void {
    if (!playing) return;
    if (call("biosim_wasm_is_gen_complete")) {
        playing = false;
        postMessage({
            type: "status",
            message: "Reached end of generation",
        } satisfies WorkerEvent);
        return;
    }
    call("biosim_wasm_do_step");
    const step = call("biosim_wasm_get_step");
    postMessage({
        type: "status",
        message: `Run step ${step}`,
    } satisfies WorkerEvent);
    if (call("biosim_wasm_is_gen_complete")) {
        playing = false;
        postMessage({
            type: "status",
            message: "Reached end of generation",
        } satisfies WorkerEvent);
    } else {
        setTimeout(playTick, 0);
    }
}

// ── Message handler ───────────────────────────────────────────────────────────

self.addEventListener("message", (e: MessageEvent<WorkerCmd>) => {
    const cmd = e.data;
    switch (cmd.type) {
        case "play":
            startTransitionIfNeeded();
            playing = true;
            postMessage({
                type: "status",
                message: "Resume running",
            } satisfies WorkerEvent);
            playTick();
            break;
        case "stop":
            playing = false;
            // mode intentionally unchanged — agents freeze at their current
            // grid positions (idle-timeout return to kinematic is future work).
            postMessage({
                type: "status",
                message: "Stop requested",
            } satisfies WorkerEvent);
            break;
        case "reset":
            playing = false;
            mode = "idle";
            startTime = performance.now();
            break;
        case "configure": {
            playing = false;
            const p = cmd.params;
            setParamInt("population", p.population);
            setParamInt("grid-size-x", p.gridSizeX);
            setParamInt("grid-size-y", p.gridSizeY);
            setParamInt("steps-per-gen", p.stepsPerGen);
            setParamInt("max-genome-len", p.maxGenomeLen);
            setParamInt("max-neurons", p.maxNeurons);
            setParamFloat("point-mutation-rate", p.pointMutationRate);
            setParamBool("sexual-reproduction", p.sexualReproduction);
            setParamBool("choose-parents-by-fitness", p.chooseParentsByFitness);
            setParamInt("los-range", p.losRange);
            setParamInt("sensor-radius", p.sensorRadius);
            setParamBool("enable-kill", p.enableKill);
            setParamFloat("responsiveness-curve-k", p.responsivenessCurveK);
            setChallengeSpec(p.challenge);
            currentChallenge = p.challenge;
            call("biosim_wasm_init");
            mode = "idle";
            startTime = performance.now();
            postMessage({
                type: "configured",
                population: p.population,
                gridSizeX: p.gridSizeX,
                gridSizeY: p.gridSizeY,
                stepsPerGen: p.stepsPerGen,
            } satisfies WorkerEvent);
            break;
        }
        case "step":
            doStep();
            break;
        case "stepAgent":
            doStepAgent();
            break;
        case "nextGeneration":
            doNextGeneration();
            break;
        case "canvas": {
            const offscreen = cmd.canvas;
            challengeOverlayColor = cmd.overlayColor;
            challengeBorderColor = cmd.borderColor;
            if (layout) {
                offscreen.width = layout.canvasW;
                offscreen.height = layout.canvasH;
            } else if (biosim) {
                offscreen.width = call("biosim_wasm_get_size_x") * 4;
                offscreen.height = call("biosim_wasm_get_size_y") * 4;
            }
            ctx = offscreen.getContext("2d");
            hatchPattern = createHatchPattern();
            startAnimLoop();
            break;
        }
        case "layout": {
            layout = {
                canvasW: cmd.canvasW,
                canvasH: cmd.canvasH,
                gridX: cmd.gridX,
                gridY: cmd.gridY,
                gridW: cmd.gridW,
                gridH: cmd.gridH,
                gridCellsX: cmd.gridCellsX,
                gridCellsY: cmd.gridCellsY,
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
        locateFile: (filename: string) => `/wasm/${filename}`,
    });
    call("biosim_wasm_init");
    postMessage({ type: "ready" } satisfies WorkerEvent);
}

init();
