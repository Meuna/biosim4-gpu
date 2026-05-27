<script lang="ts">
    // SimConfigPanel — Simulation configuration panel.
    // All scalar knobs use ParamSlider; grid size uses GridSizeControl.
    // Presets and sliders for the grid are always visible simultaneously.
    import type {
        WorkerCmd,
        SimParams,
        ChallengeSpec,
    } from "../workers/sim.worker";
    import ParamSlider from "./ParamSlider.svelte";
    import GridSizeControl from "./GridSizeControl.svelte";
    import ChallengeControl from "./ChallengeControl.svelte";

    interface Props {
        send: (cmd: WorkerCmd) => void;
    }
    const { send }: Props = $props();

    const DEFAULT_CHALLENGE: ChallengeSpec = {
        kind: "x_band",
        xMin: 0.5,
        xMax: 1.0,
        mirror: false,
    };

    const DEFAULTS: SimParams = {
        population: 3000,
        gridSizeX: 128,
        gridSizeY: 128,
        stepsPerGen: 300,
        maxGenomeLen: 24,
        maxNeurons: 5,
        pointMutationRate: 0.001,
        sexualReproduction: false,
        chooseParentsByFitness: false,
        losRange: 16,
        sensorRadius: 2,
        enableKill: false,
        responsivenessCurveK: 2.0,
        challenge: DEFAULT_CHALLENGE,
    };

    let params = $state<SimParams>({ ...DEFAULTS });
    let dirty = $state(false);

    function markDirty(): void {
        dirty = true;
    }

    function applyConfig(): void {
        // $state.snapshot() produces a plain (non-proxy) deep copy so that
        // postMessage's structured-clone can serialise the nested challenge object.
        send({
            type: "configure",
            params: $state.snapshot(params) as SimParams,
        });
        dirty = false;
    }
</script>

<div class="sim-config">
    <!-- Panel header -->
    <div class="sim-config__header">
        <p class="small-caps sim-config__eyebrow">Configuration</p>
        <h2 class="sim-config__title">Simulation</h2>
        <p class="sim-config__subtitle">biosim4-gpu / OpenCL stepper</p>
    </div>

    <!-- sim.h ────────────────────────────────────────────────────────────── -->
    <div class="section-label">
        <span class="small-caps">Simulation</span>
        <span class="sim-config__hint">sim.h</span>
    </div>

    <ParamSlider
        label="Population"
        hint="agents"
        min={100}
        max={10000}
        step={100}
        value={params.population}
        onchange={(v) => {
            params.population = v;
            markDirty();
        }}
    />

    <ParamSlider
        label="Steps per generation"
        hint="ticks"
        min={50}
        max={1000}
        step={10}
        value={params.stepsPerGen}
        onchange={(v) => {
            params.stepsPerGen = v;
            markDirty();
        }}
    />

    <div class="sim-config__spacer"></div>

    <!-- grid.h ──────────────────────────────────────────────────────────── -->
    <div class="section-label">
        <span class="small-caps">Grid</span>
        <span class="sim-config__hint">grid.h</span>
    </div>

    <GridSizeControl
        gridSizeX={params.gridSizeX}
        gridSizeY={params.gridSizeY}
        onchange={(x, y) => {
            params.gridSizeX = x;
            params.gridSizeY = y;
            markDirty();
        }}
    />

    <div class="sim-config__spacer"></div>

    <!-- genome.h ────────────────────────────────────────────────────────── -->
    <div class="section-label">
        <span class="small-caps">Genome</span>
        <span class="sim-config__hint">genome.h</span>
    </div>

    <ParamSlider
        label="Max genome length"
        hint="genes / agent"
        min={4}
        max={64}
        step={1}
        value={params.maxGenomeLen}
        onchange={(v) => {
            params.maxGenomeLen = v;
            markDirty();
        }}
    />

    <ParamSlider
        label="Max neurons"
        hint="hidden neurons"
        min={1}
        max={20}
        step={1}
        value={params.maxNeurons}
        onchange={(v) => {
            params.maxNeurons = v;
            markDirty();
        }}
    />

    <ParamSlider
        label="Mutation rate"
        hint="probability per gene"
        min={0}
        max={0.05}
        step={0.0001}
        value={params.pointMutationRate}
        format={(v) => v.toFixed(4)}
        onchange={(v) => {
            params.pointMutationRate = v;
            markDirty();
        }}
    />

    <div class="field-row sim-config__toggle-row">
        <label class="sim-config__toggle-label">
            <input
                type="checkbox"
                class="sim-config__checkbox"
                checked={params.sexualReproduction}
                onchange={(e) => {
                    params.sexualReproduction = (
                        e.target as HTMLInputElement
                    ).checked;
                    markDirty();
                }}
                aria-label="Sexual reproduction"
            />
            <span class="field-label">Sexual reproduction</span>
        </label>
    </div>

    <div class="field-row sim-config__toggle-row">
        <label class="sim-config__toggle-label">
            <input
                type="checkbox"
                class="sim-config__checkbox"
                checked={params.chooseParentsByFitness}
                onchange={(e) => {
                    params.chooseParentsByFitness = (
                        e.target as HTMLInputElement
                    ).checked;
                    markDirty();
                }}
                aria-label="Choose parents by fitness"
            />
            <span class="field-label">Choose parents by fitness</span>
        </label>
    </div>

    <div class="sim-config__spacer"></div>

    <!-- io.h ────────────────────────────────────────────────────────────── -->
    <div class="section-label">
        <span class="small-caps">I/O</span>
        <span class="sim-config__hint">io.h</span>
    </div>

    <ParamSlider
        label="LOS range"
        hint="long-probe cells"
        min={1}
        max={32}
        step={1}
        value={params.losRange}
        onchange={(v) => {
            params.losRange = v;
            markDirty();
        }}
    />

    <ParamSlider
        label="Sensor radius"
        hint="cells"
        min={1}
        max={16}
        step={1}
        value={params.sensorRadius}
        onchange={(v) => {
            params.sensorRadius = v;
            markDirty();
        }}
    />

    <div class="field-row sim-config__toggle-row">
        <label class="sim-config__toggle-label">
            <input
                type="checkbox"
                class="sim-config__checkbox"
                checked={params.enableKill}
                onchange={(e) => {
                    params.enableKill = (e.target as HTMLInputElement).checked;
                    markDirty();
                }}
                aria-label="Enable kill action"
            />
            <span class="field-label">Enable kill action</span>
        </label>
    </div>

    <ParamSlider
        label="Responsiveness curve k"
        hint="shape parameter"
        min={0.5}
        max={4.0}
        step={0.1}
        value={params.responsivenessCurveK}
        format={(v) => v.toFixed(1)}
        onchange={(v) => {
            params.responsivenessCurveK = v;
            markDirty();
        }}
    />

    <div class="sim-config__spacer"></div>

    <!-- Challenge ──────────────────────────────────────────────────────────── -->
    <div class="section-label">
        <span class="small-caps">Challenge</span>
        <span class="sim-config__hint">challenge_spec.h</span>
    </div>

    <ChallengeControl
        value={params.challenge}
        onchange={(spec) => {
            params.challenge = spec;
            markDirty();
        }}
    />

    <div class="sim-config__spacer"></div>

    <!-- Barriers placeholder ─────────────────────────────────────────────── -->
    <div class="section-label">
        <span class="small-caps">Barriers</span>
        <span class="sim-config__hint">barriers.h</span>
    </div>
    <p class="sim-config__placeholder-note">
        Composite knobs not yet implemented.
    </p>

    <div class="sim-config__spacer sim-config__spacer--lg"></div>
</div>

<!-- Sticky apply bar — stays at bottom of rail scroll area -->
<div class="sim-config__apply">
    <button
        class="button button--pill {dirty
            ? 'button--filled'
            : 'button--ghost'} sim-config__apply-btn"
        disabled={!dirty}
        onclick={applyConfig}
        aria-label="Apply configuration and restart simulation"
    >
        {dirty ? "apply & restart →" : "✓ in sync"}
    </button>
</div>

<style>
    .sim-config {
        padding: var(--space-6);
    }

    .sim-config__header {
        margin-bottom: var(--space-6);
    }

    .sim-config__eyebrow {
        margin: 0 0 var(--space-1) 0;
    }

    .sim-config__title {
        font-family: var(--font-sans);
        font-size: var(--text-2xl);
        font-weight: 700;
        line-height: 1.12;
        color: var(--color-text);
        margin: 0;
    }

    .sim-config__subtitle {
        font-family: var(--font-sans);
        font-size: 0.75rem;
        color: var(--color-text-muted);
        margin: var(--space-1) 0 0 0;
    }

    .sim-config__hint {
        font-family: var(--font-mono);
        font-size: 0.625rem;
        color: var(--color-text-muted);
    }

    .sim-config__toggle-row {
        align-items: center;
    }

    .sim-config__toggle-label {
        display: flex;
        align-items: center;
        gap: var(--space-3);
        cursor: pointer;
    }

    .sim-config__toggle-label .field-label {
        margin-bottom: 0;
    }

    .sim-config__checkbox {
        accent-color: var(--color-accent);
        width: 1rem;
        height: 1rem;
        flex-shrink: 0;
    }

    .sim-config__placeholder-note {
        font-family: var(--font-sans);
        font-size: var(--text-sm);
        color: var(--color-text-muted);
        margin: var(--space-1) 0 0 0;
        font-style: italic;
    }

    .sim-config__spacer {
        height: var(--space-2);
    }

    .sim-config__spacer--lg {
        height: var(--space-8);
    }

    /* Sticky apply bar */
    .sim-config__apply {
        position: sticky;
        bottom: 0;
        padding: var(--space-3) var(--space-6);
        background: linear-gradient(
            to top,
            var(--color-surface-alt) 70%,
            transparent
        );
        display: flex;
        gap: var(--space-2);
        z-index: 1;
    }

    .sim-config__apply-btn {
        flex: 1;
        justify-content: center;
    }
</style>
