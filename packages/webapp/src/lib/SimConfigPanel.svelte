<script lang="ts">
    // SimConfigPanel — Simulation configuration panel.
    // In "Preset" mode the panel shows non-functional preset buttons.
    // In "Custom" mode it exposes all 14 scalar knobs grouped by C header.
    import type { WorkerCmd, SimParams } from "../workers/sim.worker";

    interface Props {
        send: (cmd: WorkerCmd) => void;
    }
    const { send }: Props = $props();

    const DEFAULTS: SimParams = {
        population: 3000,
        gridSizeX: 128,
        gridSizeY: 128,
        stepsPerGen: 300,
        maxGenerations: 1000,
        maxGenomeLen: 24,
        maxNeurons: 5,
        pointMutationRate: 0.001,
        sexualReproduction: false,
        chooseParentsByFitness: false,
        losRange: 16,
        sensorRadius: 2,
        enableKill: false,
        responsivenessCurveK: 2.0,
    };

    let params = $state<SimParams>({ ...DEFAULTS });
    let mode = $state<"preset" | "custom">("preset");
    let dirty = $state(false);

    const PRESETS = [
        "default",
        "corners-128",
        "predator-256",
        "sparse",
    ] as const;

    function markDirty(): void {
        dirty = true;
    }

    function applyConfig(): void {
        send({ type: "configure", params: { ...params } });
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

    <!-- Mode toggle -->
    <div class="sim-config__mode-row">
        {#if mode === "preset"}
            <button
                class="button button--pill button--filled sim-config__mode-btn"
                disabled
                aria-current="true"
            >
                Preset
            </button>
            <button
                class="button button--pill button--ghost sim-config__mode-btn"
                onclick={() => {
                    mode = "custom";
                }}
            >
                Custom →
            </button>
        {:else}
            <button
                class="button button--pill button--ghost sim-config__mode-btn"
                onclick={() => {
                    mode = "preset";
                }}
            >
                ← Preset
            </button>
            <button
                class="button button--pill button--filled sim-config__mode-btn"
                disabled
                aria-current="true"
            >
                Custom
            </button>
        {/if}
    </div>

    {#if mode === "preset"}
        <!-- Preset buttons (non-functional placeholder) -->
        <div class="section-label">
            <span class="small-caps">Presets</span>
            <span class="sim-config__hint">4 saved</span>
        </div>
        <div
            class="sim-config__pills sim-config__pills--wrap sim-config__presets"
        >
            {#each PRESETS as p, i}
                <button
                    class="button button--pill {i === 0
                        ? 'button--filled'
                        : 'button--ghost'}">{p}</button
                >
            {/each}
        </div>
        <p class="sim-config__placeholder-note">
            Preset loading not yet implemented. Switch to Custom to edit
            individual parameters.
        </p>
    {:else}
        <!-- sim.h ────────────────────────────────────────────────────────── -->
        <div class="section-label">
            <span class="small-caps">Simulation</span>
            <span class="sim-config__hint">sim.h</span>
        </div>

        <div class="field-row">
            <div class="sim-config__field-header">
                <span class="field-label">Population</span>
                <span class="field-hint"
                    >{params.population.toLocaleString("fr-FR")} agents</span
                >
            </div>
            <div class="sim-config__slider-row">
                <input
                    type="range"
                    class="sim-config__slider"
                    min="100"
                    max="10000"
                    step="100"
                    value={params.population}
                    oninput={(e) => {
                        params.population = parseInt(
                            (e.target as HTMLInputElement).value,
                            10,
                        );
                        markDirty();
                    }}
                    aria-label="Population"
                />
                <span class="sim-config__slider-val">
                    {params.population.toLocaleString("fr-FR")}
                </span>
            </div>
        </div>

        <div class="field-row">
            <div class="sim-config__field-header">
                <span class="field-label">Steps per generation</span>
                <span class="field-hint">ticks</span>
            </div>
            <div class="sim-config__slider-row">
                <input
                    type="range"
                    class="sim-config__slider"
                    min="50"
                    max="1000"
                    step="10"
                    value={params.stepsPerGen}
                    oninput={(e) => {
                        params.stepsPerGen = parseInt(
                            (e.target as HTMLInputElement).value,
                            10,
                        );
                        markDirty();
                    }}
                    aria-label="Steps per generation"
                />
                <span class="sim-config__slider-val">{params.stepsPerGen}</span>
            </div>
        </div>

        <div class="sim-config__spacer"></div>

        <!-- grid.h ──────────────────────────────────────────────────────── -->
        <div class="section-label">
            <span class="small-caps">Grid</span>
            <span class="sim-config__hint">grid.h</span>
        </div>

        <div class="field-row">
            <span class="field-label">Size</span>
            <div class="sim-config__grid-inputs">
                <label class="sim-config__grid-label">
                    <span class="sim-config__grid-axis">X</span>
                    <input
                        type="number"
                        class="control sim-config__grid-input"
                        min="16"
                        max="512"
                        value={params.gridSizeX}
                        oninput={(e) => {
                            const v = parseInt(
                                (e.target as HTMLInputElement).value,
                                10,
                            );
                            if (!isNaN(v)) {
                                params.gridSizeX = v;
                                markDirty();
                            }
                        }}
                        aria-label="Grid width"
                    />
                </label>
                <span class="sim-config__grid-sep">×</span>
                <label class="sim-config__grid-label">
                    <span class="sim-config__grid-axis">Y</span>
                    <input
                        type="number"
                        class="control sim-config__grid-input"
                        min="16"
                        max="512"
                        value={params.gridSizeY}
                        oninput={(e) => {
                            const v = parseInt(
                                (e.target as HTMLInputElement).value,
                                10,
                            );
                            if (!isNaN(v)) {
                                params.gridSizeY = v;
                                markDirty();
                            }
                        }}
                        aria-label="Grid height"
                    />
                </label>
            </div>
        </div>

        <div class="sim-config__spacer"></div>

        <!-- genome.h ────────────────────────────────────────────────────── -->
        <div class="section-label">
            <span class="small-caps">Genome</span>
            <span class="sim-config__hint">genome.h</span>
        </div>

        <div class="field-row">
            <div class="sim-config__field-header">
                <span class="field-label">Max genome length</span>
                <span class="field-hint">genes / agent</span>
            </div>
            <div class="sim-config__slider-row">
                <input
                    type="range"
                    class="sim-config__slider"
                    min="4"
                    max="64"
                    step="1"
                    value={params.maxGenomeLen}
                    oninput={(e) => {
                        params.maxGenomeLen = parseInt(
                            (e.target as HTMLInputElement).value,
                            10,
                        );
                        markDirty();
                    }}
                    aria-label="Max genome length"
                />
                <span class="sim-config__slider-val">{params.maxGenomeLen}</span
                >
            </div>
        </div>

        <div class="field-row">
            <div class="sim-config__field-header">
                <span class="field-label">Max neurons</span>
                <span class="field-hint">hidden neurons</span>
            </div>
            <div class="sim-config__slider-row">
                <input
                    type="range"
                    class="sim-config__slider"
                    min="1"
                    max="20"
                    step="1"
                    value={params.maxNeurons}
                    oninput={(e) => {
                        params.maxNeurons = parseInt(
                            (e.target as HTMLInputElement).value,
                            10,
                        );
                        markDirty();
                    }}
                    aria-label="Max neurons"
                />
                <span class="sim-config__slider-val">{params.maxNeurons}</span>
            </div>
        </div>

        <div class="field-row">
            <div class="sim-config__field-header">
                <span class="field-label">Mutation rate</span>
                <span class="field-hint">probability per gene</span>
            </div>
            <div class="sim-config__slider-row">
                <input
                    type="range"
                    class="sim-config__slider"
                    min="0"
                    max="0.05"
                    step="0.0001"
                    value={params.pointMutationRate}
                    oninput={(e) => {
                        params.pointMutationRate = parseFloat(
                            (e.target as HTMLInputElement).value,
                        );
                        markDirty();
                    }}
                    aria-label="Mutation rate"
                />
                <span class="sim-config__slider-val"
                    >{params.pointMutationRate.toFixed(4)}</span
                >
            </div>
        </div>

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

        <!-- io.h ────────────────────────────────────────────────────────── -->
        <div class="section-label">
            <span class="small-caps">I/O</span>
            <span class="sim-config__hint">io.h</span>
        </div>

        <div class="field-row">
            <div class="sim-config__field-header">
                <span class="field-label">LOS range</span>
                <span class="field-hint">long-probe cells</span>
            </div>
            <div class="sim-config__slider-row">
                <input
                    type="range"
                    class="sim-config__slider"
                    min="1"
                    max="32"
                    step="1"
                    value={params.losRange}
                    oninput={(e) => {
                        params.losRange = parseInt(
                            (e.target as HTMLInputElement).value,
                            10,
                        );
                        markDirty();
                    }}
                    aria-label="LOS range"
                />
                <span class="sim-config__slider-val">{params.losRange}</span>
            </div>
        </div>

        <div class="field-row">
            <div class="sim-config__field-header">
                <span class="field-label">Sensor radius</span>
                <span class="field-hint">cells</span>
            </div>
            <div class="sim-config__slider-row">
                <input
                    type="range"
                    class="sim-config__slider"
                    min="1"
                    max="16"
                    step="1"
                    value={params.sensorRadius}
                    oninput={(e) => {
                        params.sensorRadius = parseInt(
                            (e.target as HTMLInputElement).value,
                            10,
                        );
                        markDirty();
                    }}
                    aria-label="Sensor radius"
                />
                <span class="sim-config__slider-val">{params.sensorRadius}</span
                >
            </div>
        </div>

        <div class="field-row sim-config__toggle-row">
            <label class="sim-config__toggle-label">
                <input
                    type="checkbox"
                    class="sim-config__checkbox"
                    checked={params.enableKill}
                    onchange={(e) => {
                        params.enableKill = (
                            e.target as HTMLInputElement
                        ).checked;
                        markDirty();
                    }}
                    aria-label="Enable kill action"
                />
                <span class="field-label">Enable kill action</span>
            </label>
        </div>

        <div class="field-row">
            <div class="sim-config__field-header">
                <span class="field-label">Responsiveness curve k</span>
                <span class="field-hint">shape parameter</span>
            </div>
            <div class="sim-config__slider-row">
                <input
                    type="range"
                    class="sim-config__slider"
                    min="0.5"
                    max="4.0"
                    step="0.1"
                    value={params.responsivenessCurveK}
                    oninput={(e) => {
                        params.responsivenessCurveK = parseFloat(
                            (e.target as HTMLInputElement).value,
                        );
                        markDirty();
                    }}
                    aria-label="Responsiveness curve k"
                />
                <span class="sim-config__slider-val"
                    >{params.responsivenessCurveK.toFixed(1)}</span
                >
            </div>
        </div>

        <div class="sim-config__spacer"></div>

        <!-- generation.h ────────────────────────────────────────────────── -->
        <div class="section-label">
            <span class="small-caps">Generations</span>
            <span class="sim-config__hint">generation.h</span>
        </div>

        <div class="field-row">
            <div class="sim-config__field-header">
                <span class="field-label">Max generations</span>
                <span class="field-hint">stop after</span>
            </div>
            <div class="sim-config__slider-row">
                <input
                    type="range"
                    class="sim-config__slider"
                    min="10"
                    max="10000"
                    step="100"
                    value={params.maxGenerations}
                    oninput={(e) => {
                        params.maxGenerations = parseInt(
                            (e.target as HTMLInputElement).value,
                            10,
                        );
                        markDirty();
                    }}
                    aria-label="Max generations"
                />
                <span class="sim-config__slider-val"
                    >{params.maxGenerations.toLocaleString("fr-FR")}</span
                >
            </div>
        </div>

        <div class="sim-config__spacer"></div>

        <!-- Challenge placeholder ────────────────────────────────────────── -->
        <div class="section-label">
            <span class="small-caps">Challenge</span>
            <span class="sim-config__hint">challenge_spec.h</span>
        </div>
        <p class="sim-config__placeholder-note">
            Composite knobs not yet implemented.
        </p>

        <div class="sim-config__spacer"></div>

        <!-- Barriers placeholder ─────────────────────────────────────────── -->
        <div class="section-label">
            <span class="small-caps">Barriers</span>
            <span class="sim-config__hint">barriers.h</span>
        </div>
        <p class="sim-config__placeholder-note">
            Composite knobs not yet implemented.
        </p>
    {/if}

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

    .sim-config__mode-row {
        display: flex;
        gap: var(--space-2);
        margin-bottom: var(--space-6);
    }

    .sim-config__mode-btn {
        flex: 1;
        justify-content: center;
    }

    .sim-config__field-header {
        display: flex;
        justify-content: space-between;
        align-items: baseline;
        margin-bottom: var(--space-2);
    }

    .sim-config__field-header .field-label {
        margin-bottom: 0;
    }

    .sim-config__slider-row {
        display: flex;
        align-items: center;
        gap: var(--space-3);
    }

    .sim-config__slider {
        flex: 1;
        accent-color: var(--color-accent);
    }

    .sim-config__slider-val {
        font-family: var(--font-mono);
        font-size: var(--text-sm);
        color: var(--color-text);
        min-width: 3.5rem;
        text-align: right;
    }

    .sim-config__pills {
        display: flex;
        gap: var(--space-2);
    }

    .sim-config__pills--wrap {
        flex-wrap: wrap;
    }

    .sim-config__presets {
        margin-bottom: var(--space-4);
    }

    .sim-config__grid-inputs {
        display: flex;
        align-items: center;
        gap: var(--space-2);
    }

    .sim-config__grid-label {
        display: flex;
        align-items: center;
        gap: var(--space-1);
    }

    .sim-config__grid-axis {
        font-family: var(--font-mono);
        font-size: var(--text-sm);
        color: var(--color-text-muted);
        min-width: 0.75rem;
    }

    .sim-config__grid-input {
        width: 5rem;
    }

    .sim-config__grid-sep {
        font-family: var(--font-mono);
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
