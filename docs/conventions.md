# Code Conventions

## Language-specific conventions

- [`docs/conventions-c.md`](conventions-c.md) — C/OpenCL: naming, error handling, alloc/goto/free
  discipline, error logging discipline, host/device portability, portability
  pitfalls, CMake, and testing. Applies to `core`, `cfgparse`, `sim-ref`,
  `sim-gpu`, and `sim-wasm`.
- [`docs/conventions-webapp.md`](conventions-webapp.md) — TypeScript/Svelte: naming, module
  organisation, no global mutable state, testing, linting, formatting, and
  webapp styling. Applies to `webapp`.

## Commit messages

### Format

```
"<scope>: <imperative description> (#<issue-number>)"
```

- `CLAUDE.md: add PR workflow section (#51)`
- `webapp: design kinematic and grid canvas feature (#48)`


### Rules

- Use the imperative mood: "add", not "added" or "adds".
- No period at the end of the description.
- Every commit must reference an issue number.
- Multiple commits per PR are allowed; each references the same issue number.
- When addressing PR review comments, group related changes into focused commits.
