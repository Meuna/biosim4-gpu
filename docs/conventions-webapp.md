# Webapp Conventions

Applies to `packages/webapp/`. Note: `packages/sim-wasm/` is C code and
follows `docs/conventions-c.md`.

## Naming

- **Modules**: `camelCase.ts`
- **Svelte components**: `PascalCase.svelte`

## Module organisation

Avoid barrel files (`index.ts` re-exports) unless a genuine package boundary
requires one.

## No global mutable state

Use Svelte stores or passed props. No module-level mutable variables.

## Testing

Each `src/lib/foo.ts` module has a mirror test file `src/lib/foo.test.ts`
using [Vitest](https://vitest.dev/). Component tests use
`@testing-library/svelte`.

```sh
bun run --cwd packages/webapp test          # non-interactive
bun run --cwd packages/webapp test:watch    # watch mode
```

## Linting and formatting

- **ESLint 9** flat config: `packages/webapp/eslint.config.js`. Run via
  `cmake --build --preset webapp --target lint`.
- **Prettier** with `prettier-plugin-svelte`. Run via
  `cmake --build --preset webapp --target format`.

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
