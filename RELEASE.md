# Release process

Directive reference for maintainer. Coding agents must implement

## 1. Model

Two **decoupled** release tracks:

- **Native binaries** `biosim-ref`, `biosim-gpu` → published as a GitHub Release,
  **driven by a `v*` tag**.
- **Webapp** → deployed to GitHub Pages on **every push to `main`** that touches
  the frontend.

## 2. Version sources

Three sources are kept on purpose, with **disjoint roles**. Do not collapse them.

| Source | Location | Role | Changes |
|---|---|---|---|
| Declared version | `project(... VERSION x.y.z)` in `CMakeLists.txt` | Source-tree's declared version (CMake hygiene) | Release commit only |
| Declared version | `"version-string"` in `vcpkg.json` | Mirror the CMake version | Release commit only |
| Build stamp | `BIOSIM_GIT_VERSION` from `git describe --tags --always --dirty` (`cmake/BuildVersion.cmake`) | Compiled into the binary; printed by `--version`; authoritative build provenance | Every commit, automatically |

Rules:

- Tag format: `vM.m.p` for release and ``vM.m.p-rcx` for pre-release.
- For a published release, the two declared numbers MUST be equal to the tag
  without the leading `v`. CI enforces this.
- Between releases, the declared numbers stay frozen and lag `git describe`.

## 3. Cutting a binary release

The **release commit metadata only, no code**. Check-list:

1. Meta-data commit(s):
   - Finalize `CHANGELOG.md`: rename `## [Unreleased]` to `## [X.Y.Z] - YYYY-MM-DD`,
     update the compare links at the bottom.
   - Bump declared version `project(... VERSION X.Y.Z)` and `"version-string"`.
   - Commit message: `Release M.m.p`.
2. `git push`. Wait for green CI on that commit.
3. Tag that exact commit (annotated, GPG-signed):
   - `git tag -s vM.m.p -m "Release M.m.p"`
   - `git push origin vX.Y.Z`
4. `release.yml` runs: builds targets and creates a **draft** release.
5. Review the draft and **Publish**.

Never tag a commit before its CI is green.

## 4. Webapp / Pages

- Deploys via `pages.yml` on push to `main`. No tag involved; the live demo
  tracks `main`.
- One-time setup: repo **Settings → Pages → Source = "GitHub Actions"**.
- Base path `/biosim4-gpu/` is injected through the `BASE_PATH` env var.
  A custom domain or hosting at the user root page `*.github.io` would
  use `BASE_PATH=/`.

## 5. CHANGELOG discipline

Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) +
[SemVer](https://semver.org/). One file: `CHANGELOG.md`.

### Structure

- The top section is always `## [Unreleased]`.
- Categories, in this fixed order, omit the empty ones: **Added, Changed,
  Deprecated, Removed, Fixed, Security**.
- Compare links sit at the bottom of the file.

### When to write

- Add the entry **in the same PR** that makes the change, under `[Unreleased]`.
  The changelog is always current, and the release commit only renames the section.

### How to write an entry — rules for humans AND coding agents

- **One line per change**, one bullet under one category.
- **Describe the user-visible behavior, not the implementation.** No file
  names, no function or kernel names, no "refactored X into Y", no rationale,
  no internal mechanics.
- **End every entry with its linked PR reference**:
  `([#NNN](https://github.com/<owner>/<repo>/pull/NNN)])`.
- Imperative or simple present, capitalized, ≤ ~120 characters, single-line, no
  trailing period.
- If a change has no user-visible effect it usually does not belong here. If it
  must be recorded, a single terse line under **Changed**.
- One PR = one line, unless it genuinely ships two distinct user-facing changes
  — then one line each, both ending with the same link `(#NNN)`.

### Examples

Good:

```markdown
### Added
- Snapshot checkpointing with `--snapshot-in`, `--snapshot-out`
  ([#105](https://github.com/meuna/biosim4-gpu/pull/105))
- Brain Explorer view with force-directed neuron layout
  ([#75](https://github.com/meuna/biosim4-gpu/pull/75))

### Fixed
- Build and run on Windows x64 (MSVC)
  ([#107](https://github.com/meuna/biosim4-gpu/pull/107))
```

Rejected (a novel, implementation detail, no PR link):

```markdown
### Added
- Implemented a new OpenCL kernel k1_feedforward.cl computing the feedforward
  pass; reworked runner.c to dispatch it via clEnqueueNDRangeKernel and switched
  weights to an SoA layout because the previous AoS layout caused warp divergence
  on large connection counts...
```
