// biosim.mjs is a pre-built Emscripten ES6 module. @sim-wasm resolves to the
// sim-wasm build output directory (outside publicDir), so Vite's ?url import
// is accepted. locateFile pins biosim.wasm to its publicDir URL independently
// of where biosim.mjs is served (it gets a hashed URL in production builds).
import biosimUrl from "@sim-wasm/biosim.mjs?url";

interface EmscriptenModule {
    ccall(
        name: string,
        returnType: "number" | null,
        argTypes: string[],
        args: unknown[],
    ): number;
}

interface EmscriptenOptions {
    locateFile?: (path: string) => string;
}

type EmscriptenFactory = (
    opts?: EmscriptenOptions,
) => Promise<EmscriptenModule>;

export type WorkerCmd =
    | { type: "play" }
    | { type: "stop" }
    | { type: "step" }
    | { type: "stepAgent" }
    | { type: "nextGeneration" };

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
    | { type: "error"; message: string };

let biosim: EmscriptenModule | null = null;
let playing = false;

function call(name: string): number {
    return biosim!.ccall(name, "number", [], []);
}

function doStep(): void {
    call("biosim_wasm_do_step");
    const step = call("biosim_wasm_get_step");
    postMessage({
        type: "status",
        message: `Run step ${step}`,
    } satisfies WorkerEvent);
}

function doStepAgent(): void {
    call("biosim_wasm_do_step_agent");
    const agent = call("biosim_wasm_get_last_agent");
    const step = call("biosim_wasm_get_step");
    postMessage({
        type: "status",
        message: `Run a agent ${agent} at step ${step}`,
    } satisfies WorkerEvent);
}

function doNextGeneration(): void {
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

self.addEventListener("message", (e: MessageEvent<WorkerCmd>) => {
    const cmd = e.data;
    switch (cmd.type) {
        case "play":
            playing = true;
            postMessage({
                type: "status",
                message: "Resume running",
            } satisfies WorkerEvent);
            playTick();
            break;
        case "stop":
            playing = false;
            postMessage({
                type: "status",
                message: "Stop requested",
            } satisfies WorkerEvent);
            break;
        case "step":
            doStep();
            break;
        case "stepAgent":
            doStepAgent();
            break;
        case "nextGeneration":
            doNextGeneration();
            break;
    }
});

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
