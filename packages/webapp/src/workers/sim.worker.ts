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

/** Scalar parameters that can be configured before (re-)initialising the sim. */
export interface SimParams {
    population: number;
    gridSizeX: number;
    gridSizeY: number;
    stepsPerGen: number;
    maxGenerations: number;
    maxGenomeLen: number;
    maxNeurons: number;
    pointMutationRate: number;
    sexualReproduction: boolean;
    chooseParentsByFitness: boolean;
    losRange: number;
    sensorRadius: number;
    enableKill: boolean;
    responsivenessCurveK: number;
}

export type WorkerCmd =
    | { type: "play" }
    | { type: "stop" }
    | { type: "reset" }
    | { type: "step" }
    | { type: "stepAgent" }
    | { type: "nextGeneration" }
    | { type: "configure"; params: SimParams }
    | { type: "canvas"; canvas: OffscreenCanvas }
    | {
          type: "layout";
          canvasW: number;
          canvasH: number;
          gridX: number;
          gridY: number;
          gridSize: number;
          gridCells: number;
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
          stepsPerGen: number;
      }
    | { type: "error"; message: string };

// ── Rendering mode ────────────────────────────────────────────────────────────

type Mode = "idle" | "transitioning-in" | "running";

// ── Module-level state ────────────────────────────────────────────────────────

let biosim: EmscriptenModule | null = null;
let playing = false;
let ctx: OffscreenCanvasRenderingContext2D | null = null;

interface Layout {
    canvasW: number;
    canvasH: number;
    gridX: number;
    gridY: number;
    gridSize: number;
    gridCells: number;
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

    const { gridX, gridY, gridSize, gridCells } = layout;
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
                gridSize,
                gridCells,
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

    const { canvasW, canvasH, gridX, gridY, gridSize, gridCells } = layout;
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
            const to = gridPosition(gx, gy, gridX, gridY, gridSize, gridCells);
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
            setParamInt("max-generations", p.maxGenerations);
            setParamInt("max-genome-len", p.maxGenomeLen);
            setParamInt("max-neurons", p.maxNeurons);
            setParamFloat("point-mutation-rate", p.pointMutationRate);
            setParamBool("sexual-reproduction", p.sexualReproduction);
            setParamBool("choose-parents-by-fitness", p.chooseParentsByFitness);
            setParamInt("los-range", p.losRange);
            setParamInt("sensor-radius", p.sensorRadius);
            setParamBool("enable-kill", p.enableKill);
            setParamFloat("responsiveness-curve-k", p.responsivenessCurveK);
            call("biosim_wasm_init");
            mode = "idle";
            startTime = performance.now();
            postMessage({
                type: "configured",
                population: p.population,
                gridSizeX: p.gridSizeX,
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
            if (layout) {
                offscreen.width = layout.canvasW;
                offscreen.height = layout.canvasH;
            } else if (biosim) {
                offscreen.width = call("biosim_wasm_get_size_x") * 4;
                offscreen.height = call("biosim_wasm_get_size_y") * 4;
            }
            ctx = offscreen.getContext("2d");
            startAnimLoop();
            break;
        }
        case "layout": {
            layout = {
                canvasW: cmd.canvasW,
                canvasH: cmd.canvasH,
                gridX: cmd.gridX,
                gridY: cmd.gridY,
                gridSize: cmd.gridSize,
                gridCells: cmd.gridCells,
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
