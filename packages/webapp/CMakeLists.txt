find_program(BUN_EXECUTABLE bun
  HINTS "$ENV{HOME}/.bun/bin" "$ENV{BUN_INSTALL}/bin"
  REQUIRED
)

set(WASM_BINARY_DIR "${CMAKE_BINARY_DIR}/packages/sim-wasm")
set(PUBLIC_DIR "${CMAKE_CURRENT_BINARY_DIR}/public")

# sim-wasm wiring to webapp happens here. See vite.config.js for the corresponding vite wiring.
add_custom_target(copy-wasm-into-public-dir
  DEPENDS biosim-wasm
  COMMAND ${CMAKE_COMMAND} -E make_directory "${PUBLIC_DIR}/wasm"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${WASM_BINARY_DIR}/biosim.wasm"
          "${PUBLIC_DIR}/wasm/"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_CURRENT_SOURCE_DIR}/public/favicon.ico"
          "${CMAKE_CURRENT_SOURCE_DIR}/public/favicon.svg"
          "${CMAKE_CURRENT_SOURCE_DIR}/public/apple-touch-icon.png"
          "${CMAKE_CURRENT_SOURCE_DIR}/public/icon-32.png"
          "${PUBLIC_DIR}/"
)

add_custom_target(webapp ALL
  DEPENDS copy-wasm-into-public-dir
  COMMAND ${BUN_EXECUTABLE} install --frozen-lockfile
  COMMAND ${CMAKE_COMMAND} -E
          env "CMAKE_CURRENT_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
          env "WASM_BINARY_DIR=${WASM_BINARY_DIR}"
          ${BUN_EXECUTABLE} run build
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Building webapp (Bun + Vite + Svelte)"
)

# Dev server — not in ALL; only runs when requested with --target dev.
add_custom_target(dev
  DEPENDS copy-wasm-into-public-dir
  COMMAND ${CMAKE_COMMAND} -E
          env "CMAKE_CURRENT_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
          env "WASM_BINARY_DIR=${WASM_BINARY_DIR}"
          ${BUN_EXECUTABLE} run dev
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Starting Vite dev server"
  USES_TERMINAL
  VERBATIM
)
