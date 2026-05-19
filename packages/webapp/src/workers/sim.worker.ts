// biosim.mjs is a pre-built Emscripten ES6 module. @sim-wasm resolves to the
// sim-wasm build output directory (outside publicDir), so Vite's ?url import
// is accepted. locateFile pins biosim.wasm to its publicDir URL independently
// of where biosim.mjs is served (it gets a hashed URL in production builds).
import biosimUrl from "@sim-wasm/biosim.mjs?url";

interface EmscriptenModule {
    ccall(
        name: string,
        returnType: null,
        argTypes: string[],
        args: unknown[],
    ): void;
}

interface EmscriptenOptions {
    locateFile?: (path: string) => string;
}

type EmscriptenFactory = (
    opts?: EmscriptenOptions,
) => Promise<EmscriptenModule>;

async function init(): Promise<void> {
    const mod = await import(/* @vite-ignore */ biosimUrl);
    const createBiosim = mod.default as EmscriptenFactory;
    const biosim = await createBiosim({
        locateFile: (filename: string) => `/wasm/${filename}`,
    });
    biosim.ccall("biosim_hello", null, [], []);
    postMessage("ready");
}

init();
