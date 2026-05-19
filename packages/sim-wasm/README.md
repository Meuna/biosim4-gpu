# sim-wasm

Emscripten-compiled WebAssembly module that exposes the biosim4-gpu simulation
engine to JavaScript. It is built as an ES6 module (`biosim.mjs` + `biosim.wasm`)
with `-sMODULARIZE=1 -sEXPORT_ES6=1 -sENVIRONMENT=worker` so that it can be
loaded inside a Web Worker via a dynamic `import()`. The module exports a default
factory function; call it with `await createBiosim()` to obtain an instance, then
invoke C functions through `ccall` / `cwrap`.
