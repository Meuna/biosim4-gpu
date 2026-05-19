import { defineConfig, searchForWorkspaceRoot } from 'vite'
import { svelte } from '@sveltejs/vite-plugin-svelte'
import path from 'path'

const binaryDir = process.env.CMAKE_CURRENT_BINARY_DIR!
const wasmBinaryDir = process.env.WASM_BINARY_DIR!

export default defineConfig({
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
