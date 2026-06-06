# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with
code in this repository.

## Project

GPU port of [biosim4](https://github.com/davidrmiller/biosim4) (by David R.
Miller) using OpenCL. Six packages are implemented: `core`, `cfgparse`,
`sim-ref`, `sim-gpu`, `sim-wasm`, and `webapp`.

## Two build branches

This repo has two independent build trees. Every task belongs to one or both.
**Identify your branch before starting work.**

| Branch | Packages | CMake presets | When it applies |
|---|---|---|---|
| **Native** | `core`, `cfgparse`, `sim-ref`, `sim-gpu` | `debug` `release` `asan` `ci` | Any C/H/CL file outside `packages/sim-wasm/` and `packages/webapp/` |
| **Webapp** | `sim-wasm`, `webapp` | `webapp` | Any file under `packages/sim-wasm/` or `packages/webapp/` |

A change touching files from both branches requires the quality sequence to be
run on **both** branches.

> `sim-wasm` is C code, but its lint target lives in the webapp preset — treat
> it as the webapp branch for all quality-check purposes.

### Quality check sequence

The sequence is the same for both branches:

1. **build** — must compile with zero errors
2. **test** — all tests must pass
3. **lint** — fix every error/warning (see cognitive-complexity note below)
4. **format** — apply formatting
5. **build** — confirm format did not break compilation
6. **test** — confirm format did not break tests

A task is **NOT** complete while lint reports any error or warning.

Branch-specific commands:

| Step | Native (`--preset debug`) | Webapp (`--preset webapp`) |
|---|---|---|
| build | `cmake --build --preset debug` | `cmake --build --preset webapp` |
| test | `ctest --preset debug` | `bun run --cwd packages/webapp test` |
| lint | `cmake --build --preset debug --target lint` | `cmake --build --preset webapp --target lint` |
| format | `cmake --build --preset debug --target format` | `cmake --build --preset webapp --target format` |

### `readability-function-cognitive-complexity` special case

When facing a cognitive-complexity lint error, do not fix blindly. Ask the user
to decide whether to refactor or add a `NOLINTNEXTLINE` flag.

## GH Issue PR Workflow

When asked to implement a GitHub issue, follow this workflow exactly.

### 1. Understand the issue

Read the full specification before writing any code:

```sh
gh issue view {number}
```

### 2. Create a feature branch

```sh
git checkout -b gh-{number}
```

Branch names must follow the pattern `gh-{number}`.

### 3. Implement and commit

Commit incrementally as work progresses. Multiple commits per PR are allowed
and encouraged for large changes. Use the commit format defined in
`docs/conventions.md`:

```
"<scope>: <imperative description> (#<issue-number>)"
```

### 4. Quality check

Run the full quality check sequence (build → test → lint → format → build →
test) for every affected build branch, exactly as defined in
[Two build branches](#two-build-branches). Include the webapp dev check if any
webapp files were changed.

### 5. Push and open a PR

```sh
git push -u origin gh-{number}
gh pr create \
  --title "gh-{N}: {one-line summary}" \
  --body "Closes #{N}

{description of what was implemented and key decisions}"
```

The PR body **must** contain `Closes #{N}` so that GitHub closes the issue
automatically when the PR is merged.

### 6. Respond to review comments

IMPORTANT: respond to the latest review ONLY, or a specific review ID when provided.

1. If not provided, find the latest review ID:
   `LATEST_REVIEW=$(gh api repos/<owner>/<repo>/pulls/<pr>/reviews --jq '.[-1].id')`
2. Read the main review comment:
   `gh api repos/<owner>/<repo>/pulls/<pr>/reviews --jq ".[] | select(.id == $LATEST_REVIEW) | .body"`
3. Read the inline comments:
   `gh api repos/<owner>/<repo>/pulls/<pr>/comments --jq ".[] | select(.pull_request_review_id == $LATEST_REVIEW) | {id,path,line,start_line,side,body}"`
4. Implement the review, group related review changes into focused commits.
5. Push: `git push`

Do not squash or amend commits during review response. Preserve the atomic
trail so reviewers can track changes individually.

## Working with this repository

- **Keep the status up-to-date.**

- **Keep the changelog up-to-date.**

- **Keep the documentation up-to-date.** When implementing a feature, update the
  documentation so it describes the system as it exists after your change. Remove
  any outdated statements.

- **Each new source module needs a test module.**
  - _Native branch:_ when adding `packages/core/src/foo.c`, also add
    `packages/core/tests/test_foo.c` and register it in
    `packages/core/tests/CMakeLists.txt`. Use the Unity framework
    (see `docs/conventions-c.md`).
  - _Webapp branch:_ when adding `packages/webapp/src/lib/foo.ts`, also add
    `packages/webapp/src/lib/foo.test.ts`. Use Vitest; component tests use
    `@testing-library/svelte`.

- **Increment the snapshot schema version** when modifying `biosim_sensor_t` or
  `biosim_action_t` in `io_defs.h`.

- **Increment the snapshot format version** when modifying the snapshot interface.

- **Conventions are normative.** The rules in `docs/conventions.md` and its
  sub-files apply to every file written in this repository.

## Key Files

- `docs/architecture.md` — package structure, key types and functions
- `docs/build.md` — build system overview; sub-guides for native, OpenCL, wasm, and webapp
- `docs/conventions.md` — commit/PR conventions; sub-guides for C and webapp conventions
- `docs/formats.md` — snapshot binary format, TOML parameter format
- `docs/usage.md` — CLI reference, challenges, barriers
- `docs/gpu-design.md` — GPU kernel pipeline design

## Third Party Files

Vendored libraries in `third_party/` are read-only, except when instructed to
bump the version.
