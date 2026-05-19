#include <emscripten.h>
#include <stdio.h>

EMSCRIPTEN_KEEPALIVE void biosim_hello(void) {
    printf("biosim wasm: hello from C\n");
    emscripten_log(EM_LOG_CONSOLE, "biosim wasm: structured log");
}
