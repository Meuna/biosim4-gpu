# Data Formats

## Snapshot binary format

Snapshots capture the survivor genomes of completed generations. They are
written by `sim-stepper` after challenge evaluation and before reproduction,
at the configured interval (`--snapshot-interval`).

The format is binary, little-endian throughout.

### File layout

```
┌────────────────────────────────┐
│ FILE HEADER  (32 bytes)        │
├────────────────────────────────┤
│ GENERATION RECORD 0            │
│ GENERATION RECORD 1            │
│ …                              │
└────────────────────────────────┘
```

### File header (32 bytes)

| Offset | Size | Field | Notes |
|--------|------|-------|-------|
| 0 | 4 | magic | `0x42 0x53 0x4D 0x34` ("BSM4") |
| 4 | 2 | format_version | `BIOSIM_SNAP_FORMAT_VERSION` (current: 2) |
| 6 | 2 | schema_version | `BIOSIM_IO_SCHEMA_VERSION` (from `io_catalogue.h`) |
| 8 | 2 | num_sensors | `BIOSIM_NUM_SENSORS` at write time |
| 10 | 2 | num_actions | `BIOSIM_NUM_ACTIONS` at write time |
| 12 | 2 | genome_max_len | `sim->genome.max_len` at write time |
| 14 | 1 | max_neurons | `sim->nnet.max_neurons` at write time |
| 15 | 1 | reserved | zero |
| 16 | 4 | generation_count | number of records; 0 = unknown / streaming |
| 20 | 12 | reserved | zeros |

`format_version` is incremented when the on-disk record layout changes.
`schema_version` is incremented when the sensor/action catalogue indices change.
A schema version mismatch is fatal on load — catalogue indices would silently
corrupt gene interpretation.

### Generation record

Let `n = n_survivors` and `L = genome_max_len` (from file header).

| Offset | Size | Field | Type | Notes |
|--------|------|-------|------|-------|
| 0 | 8 | record_size | uint64_t | total bytes including this field |
| 8 | 4 | gen | uint32_t | generation index |
| 12 | 4 | n_survivors | uint32_t | |
| 16 | 8 | gen_rng | uint64_t | gen_rng state captured after survivor collection |
| 24 | n × 2 | genome_length | uint16_t[] | active gene count per survivor |
| 24 + n×2 | L × n × 2 | genome_conn | uint16_t[] | SoA: gene-slot-major |
| 24 + n×2 + L×n×2 | L × n × 2 | genome_wgt | int16_t[] | SoA: gene-slot-major |
| 24 + n×2 + 2×L×n×2 | n × 4 | score | float[] | challenge score per survivor |

Genome arrays use the transposed SoA layout: gene slot `j` of survivor `s`
is at index `j * n_survivors + s`.

`gen_rng` is captured after `biosim_generation_collect_survivors` and before
`biosim_generation_reproduce`. Restoring `sim->gen_rng` to this value before
calling reproduce yields a deterministic replay.

### Approximate sizes

At defaults (pop = 3000, 750 survivors at 25%, `genome_max_len = 24`):
- ~75 KB per generation record
- 1000 generations at interval 1 → ~72 MB; at interval 10 → ~7.2 MB

### Restore pattern

```
biosim_snapshot_restore(path, &sim)
```

Loads the last generation record, calls `biosim_generation_reproduce` exactly as
a normal generation boundary would, and resets generation counters. No special
simulation loop path is needed.

## TOML parameter format

The TOML config file structure mirrors the `biosim_param_entry_t` table of
the simulator. Top-level parameters are global keys; others are grouped under
named tables.

```toml
# Top-level (no table)
# (none in sim-stepper)

[simulation]
population      = 3000
grid-size-x     = 128
...

[genome]
max-genome-len  = 24
...

[sensors]
...

[actions]
...

[challenge]
kind  = "x_band"
...

[snapshot]
out      = "run.snap"
interval = 10
```

For a full parameter listing see [`docs/usage.md`](usage.md).

Barriers use a separate format (`[barriers]` + `[barrier-N]` tables) because
they are a variable-length array of shapes, not flat key-value parameters. See
[`docs/usage.md`](usage.md#barrier-types).

Unknown keys in the TOML file are silently ignored (forward-compatible reads).
