# webapp

Svelte 5 SPA. Loads `biosim.mjs` + `biosim.wasm` (from `sim-wasm`) inside a
Web Worker (`src/workers/sim.worker.ts`). CMake copies WASM artifacts into
`public/wasm/` before any build or dev invocation. Tooling: Vite 6, Bun, ESLint
9 (flat config), Prettier, Vitest.

**Build branch: Webapp.**

## Quality check sequence

```sh
cmake --build --preset webapp
bun run --cwd packages/webapp test
cmake --build --preset webapp --target lint
cmake --build --preset webapp --target format
cmake --build --preset webapp
bun run --cwd packages/webapp test
```

A task is **NOT** complete while lint reports any error or warning.

## Browser testing (required for every webapp task)

### Setup — install chrome-devtools-mcp

```sh
claude mcp add chrome-devtools --scope user bunx chrome-devtools-mcp@latest
```

Restart Claude Code after adding the server so it is available in the session.

### Webapp dev check

After completing the quality check sequence, verify the app works correctly in
a real browser:

1. Run `cmake --build --preset webapp --target dev`
2. Load `http://localhost:5173` via chrome-devtools-mcp
3. Confirm **zero console errors** and **zero console warnings**

A webapp task is **NOT** complete if the browser console shows any error or
warning introduced by your change.

Terminate the dev server and MCP tab when done.

## Naming

- **Modules**: `camelCase.ts`
- **Svelte components**: `PascalCase.svelte`

## No global mutable state

Use Svelte stores or passed props. No module-level mutable variables.

## No barrel files

Avoid `index.ts` re-exports unless a genuine package boundary requires one.

## Testing

`src/lib/foo.ts` → `src/lib/foo.test.ts` using Vitest. Component tests use
`@testing-library/svelte`.

## Styling

- Style with scoped CSS in `.svelte` `<style>` blocks. Do not add Tailwind or
  any utility-class framework.
- Reference semantic token aliases from `src/styles/tokens.css`. Do not
  hardcode colors, spacings, or font names in component styles.
- To change the theme, edit `src/styles/tokens.css` only. Never retheme by
  editing component styles.
- Retheme the whole app by swapping `--_accent` (and the surface/text raw
  values if needed). Verify contrast remains high after any palette change.
- Two-tier CSS framework: element defaults belong in `src/styles/base.css`;
  shared, reusable component classes (`.button`, `.panel`, `.field-row`, etc.)
  belong in `src/styles/primitives.css`, imported from `app.css` after
  `base.css`. Only component-specific styles belong in a component's scoped
  `<style>` block. **Any style pattern used by 2 or more components MUST live
  in `primitives.css`** — never duplicated inline.

## Further reading

- `docs/conventions-webapp.md` — full TypeScript/Svelte and styling conventions
- `docs/build-webapp.md` — build commands reference
- `docs/architecture.md` — webapp architecture
