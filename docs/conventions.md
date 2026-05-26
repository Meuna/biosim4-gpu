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
gh-{N}: {imperative description}
```

where `{N}` is the GitHub issue number. Examples:

- `gh-51: add PR workflow section to CLAUDE.md`
- `gh-48: design kinematic and grid canvas feature`

### Rules

- Use the imperative mood: "add", not "added" or "adds".
- No period at the end of the description.
- Every commit must reference an issue number.
- Multiple commits per PR are allowed; each references the same issue number.
- Do **not** put `closes` in commit messages — use `Closes #{N}` in the PR
  body instead so that GitHub closes the issue on merge.

### Review-response commits

When addressing PR review comments, group related changes into focused commits.
Use the same format:

```
gh-{N}: {description of what changed in response to review}
```
