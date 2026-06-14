import { defineConfig, searchForWorkspaceRoot } from 'vite'
import { svelte } from '@sveltejs/vite-plugin-svelte'
import path from 'path'

// Use these vars to serve out of tree builds on the dev server
const binaryDir = process.env.CMAKE_CURRENT_BINARY_DIR!
const wasmBinaryDir = process.env.WASM_BINARY_DIR!

// Public base path.
//   - Default '/' for local dev and the normal binary-tree build (unchanged)
//   - The Pages workflow exports BASE_PATH=/biosim4-gpu/
const base = process.env.BASE_PATH || '/'

export default defineConfig({
  base,
  plugins: [svelte()],
  server: {
    host: process.env.DEV_HOST ? '0.0.0.0' : 'localhost',
    fs: { allow: [searchForWorkspaceRoot(process.cwd()), wasmBinaryDir] },
  },
  resolve: {
    alias: { '@sim-wasm': wasmBinaryDir },
  },
  publicDir: path.join(binaryDir, 'public'),
  build: {
    outDir: path.join(binaryDir, 'dist'),
    emptyOutDir: true,
  },
  worker: {
    format: 'es',
  },
})
