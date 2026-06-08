<script lang="ts">
    // SimConfigPanel — Simulation configuration panel.
    // All scalar knobs use ParamSlider; grid size uses GridSizeControl.
    // Presets and sliders for the grid are always visible simultaneously.
    import type { SimParams } from "../workers/sim.worker";
    import ParamSlider from "./ParamSlider.svelte";
    import GridSizeControl from "./GridSizeControl.svelte";
    import ChallengeControl from "./ChallengeControl.svelte";
    import BarrierControl from "./BarrierControl.svelte";
    import { Download, FileUp, TriangleAlert, Undo2 } from "lucide-svelte";

    interface Props {
        draftConfig: SimParams;
        isDirty: boolean;
        onDraftChange: (params: SimParams) => void;
        onRevert: () => void;
        incompatibleFields?: string[];
        genomeMaxLenUsed?: number;
        genomeMaxNeuronsUsed?: number;
        onConfUpload: () => void;
        onConfDownload: () => void;
        snapReady?: boolean;
        onSnapUpload: () => void;
        onSnapDownload: () => void;
    }
    const {
        draftConfig,
        isDirty,
        onDraftChange,
        onRevert,
        incompatibleFields = [],
        genomeMaxLenUsed = 0,
        genomeMaxNeuronsUsed = 0,
        onConfUpload,
        onConfDownload,
        snapReady = false,
        onSnapUpload,
        onSnapDownload,
    }: Props = $props();
</script>

<div class="sim-config">
    <!-- Panel header -->
    <div class="sim-config__header">
        <p class="small-caps sim-config__eyebrow">Configuration</p>
        <div class="sim-config__title-row">
            <h2 class="sim-config__title">Simulation</h2>
            {#if isDirty}
                <button
                    class="button button--utility sim-config__revert"
                    onclick={onRevert}
                    aria-label="Revert all changes"
                >
                    <Undo2 size={14} /> revert
                </button>
            {/if}
        </div>
        <p class="sim-config__subtitle">biosim4-gpu / OpenCL stepper</p>
        <div class="sim-config__io-row">
            <span class="small-caps sim-config__io-label">Conf</span>
            <button
                class="button button--utility"
                onclick={onConfUpload}
                aria-label="Upload config"
            >
                <FileUp size={14} />
            </button>
            <button
                class="button button--utility"
                onclick={onConfDownload}
                aria-label="Download config"
            >
                <Download size={14} />
            </button>
            <span class="small-caps sim-config__io-label">Snapshot</span>
            <button
                class="button button--utility"
                disabled={!snapReady}
                onclick={onSnapUpload}
                aria-label="Upload snapshot"
            >
                <FileUp size={14} />
            </button>
            <button
                class="button button--utility"
                disabled={!snapReady}
                onclick={onSnapDownload}
                aria-label="Download snapshot"
            >
                <Download size={14} />
            </button>
        </div>
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
        value={draftConfig.population}
        onchange={(v) => {
            onDraftChange({ ...draftConfig, population: v });
        }}
    />

    <ParamSlider
        label="Steps per generation"
        hint="ticks"
        min={50}
        max={1000}
        step={10}
        value={draftConfig.stepsPerGen}
        onchange={(v) => {
            onDraftChange({ ...draftConfig, stepsPerGen: v });
        }}
    />

    <div class="sim-config__spacer"></div>

    <!-- grid.h ──────────────────────────────────────────────────────────── -->
    <div class="section-label">
        <span class="small-caps">Grid</span>
        <span class="sim-config__hint">grid.h</span>
    </div>

    <GridSizeControl
        gridSizeX={draftConfig.gridSizeX}
        gridSizeY={draftConfig.gridSizeY}
        onchange={(x, y) => {
            onDraftChange({ ...draftConfig, gridSizeX: x, gridSizeY: y });
        }}
    />

    <div class="sim-config__spacer"></div>

    <!-- genome.h ────────────────────────────────────────────────────────── -->
    <div class="section-label">
        <span class="small-caps">Genome</span>
        <span class="sim-config__hint">genome.h</span>
    </div>

    <div
        class="sim-config__field-wrap"
        class:sim-config__field-wrap--invalid={incompatibleFields.includes(
            "maxGenomeLen",
        )}
    >
        <ParamSlider
            label="Max genome length"
            hint="genes / agent"
            min={4}
            max={64}
            step={1}
            value={draftConfig.maxGenomeLen}
            onchange={(v) => {
                onDraftChange({ ...draftConfig, maxGenomeLen: v });
            }}
        />
        {#if incompatibleFields.includes("maxGenomeLen")}
            <p class="sim-config__incompat-hint">
                Survivors use up to {genomeMaxLenUsed} — increase to re-enable play.
            </p>
        {/if}
    </div>

    <div
        class="sim-config__field-wrap"
        class:sim-config__field-wrap--invalid={incompatibleFields.includes(
            "maxNeurons",
        )}
    >
        <ParamSlider
            label="Max neurons"
            hint="hidden neurons"
            min={1}
            max={20}
            step={1}
            value={draftConfig.maxNeurons}
            onchange={(v) => {
                onDraftChange({ ...draftConfig, maxNeurons: v });
            }}
        />
        {#if incompatibleFields.includes("maxNeurons")}
            <p class="sim-config__incompat-hint">
                Survivors use up to {genomeMaxNeuronsUsed} — increase to re-enable
                play.
            </p>
        {/if}
    </div>

    <ParamSlider
        label="Mutation rate"
        hint="probability per gene"
        min={0}
        max={0.05}
        step={0.0001}
        value={draftConfig.pointMutationRate}
        format={(v) => v.toFixed(4)}
        onchange={(v) => {
            onDraftChange({ ...draftConfig, pointMutationRate: v });
        }}
    />

    <div class="field-row toggle-row">
        <label class="toggle-label">
            <input
                type="checkbox"
                class="toggle-checkbox"
                checked={draftConfig.sexualReproduction}
                onchange={(e) => {
                    onDraftChange({
                        ...draftConfig,
                        sexualReproduction: (e.target as HTMLInputElement)
                            .checked,
                    });
                }}
                aria-label="Sexual reproduction"
            />
            <span class="field-label">Sexual reproduction</span>
        </label>
    </div>

    <div class="field-row toggle-row">
        <label class="toggle-label">
            <input
                type="checkbox"
                class="toggle-checkbox"
                checked={draftConfig.chooseParentsByFitness}
                onchange={(e) => {
                    onDraftChange({
                        ...draftConfig,
                        chooseParentsByFitness: (e.target as HTMLInputElement)
                            .checked,
                    });
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
        value={draftConfig.losRange}
        onchange={(v) => {
            onDraftChange({ ...draftConfig, losRange: v });
        }}
    />

    <ParamSlider
        label="Sensor radius"
        hint="cells"
        min={1}
        max={16}
        step={1}
        value={draftConfig.sensorRadius}
        onchange={(v) => {
            onDraftChange({ ...draftConfig, sensorRadius: v });
        }}
    />

    <div class="field-row toggle-row">
        <label class="toggle-label">
            <input
                type="checkbox"
                class="toggle-checkbox"
                checked={draftConfig.enableKill}
                onchange={(e) => {
                    onDraftChange({
                        ...draftConfig,
                        enableKill: (e.target as HTMLInputElement).checked,
                    });
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
        value={draftConfig.responsivenessCurveK}
        format={(v) => v.toFixed(1)}
        onchange={(v) => {
            onDraftChange({ ...draftConfig, responsivenessCurveK: v });
        }}
    />

    <div class="sim-config__spacer"></div>

    <!-- Challenge ──────────────────────────────────────────────────────────── -->
    <div class="section-label">
        <span class="small-caps">Challenge</span>
        <span class="sim-config__hint">challenge_spec.h</span>
    </div>

    <ChallengeControl
        value={draftConfig.challenge}
        onchange={(spec) => {
            onDraftChange({ ...draftConfig, challenge: spec });
        }}
    />

    {#if (draftConfig.challenge.kind === "near_barrier" || draftConfig.challenge.kind === "location_sequence") && draftConfig.barriers.length === 0}
        <p class="sim-config__barrier-warning">
            <TriangleAlert size={14} style="display: inline-block;" />
            This challenge requires at least one barrier — add one in the Barriers
            section below.
        </p>
    {/if}

    <div class="sim-config__spacer"></div>

    <!-- Barriers ────────────────────────────────────────────────────────────── -->
    <div class="section-label">
        <span class="small-caps">Barriers</span>
        <span class="sim-config__hint">barriers.h</span>
    </div>

    <BarrierControl
        value={draftConfig.barriers}
        challengeKind={draftConfig.challenge.kind}
        onchange={(b) => {
            onDraftChange({ ...draftConfig, barriers: b });
        }}
    />

    <div class="sim-config__spacer sim-config__spacer--lg"></div>
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

    .sim-config__title-row {
        display: flex;
        align-items: baseline;
        gap: var(--space-3);
    }

    .sim-config__title {
        font-family: var(--font-sans);
        font-size: var(--text-2xl);
        font-weight: 700;
        line-height: 1.12;
        color: var(--color-text);
        margin: 0;
    }

    .sim-config__revert {
        margin-left: auto;
        display: inline-flex;
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

    .sim-config__barrier-warning {
        font-family: var(--font-sans);
        font-size: var(--text-sm);
        color: var(--color-warn);
        margin: var(--space-2) 0 0 0;
    }

    .sim-config__field-wrap {
        padding-left: var(--space-2);
        border-left: 2px solid transparent;
        transition: border-color 0.1s;
    }

    .sim-config__field-wrap--invalid {
        border-left-color: var(--color-warn);
    }

    .sim-config__incompat-hint {
        font-family: var(--font-mono);
        font-size: 0.625rem;
        color: var(--color-warn);
        margin: calc(-1 * var(--space-2)) 0 var(--space-4) 0;
    }

    .sim-config__spacer {
        height: var(--space-2);
    }

    .sim-config__spacer--lg {
        height: var(--space-8);
    }

    .sim-config__io-row {
        display: flex;
        align-items: center;
        gap: var(--space-1);
        margin-top: var(--space-2);
    }

    .sim-config__io-label {
        font-size: 0.625rem;
    }
</style>
