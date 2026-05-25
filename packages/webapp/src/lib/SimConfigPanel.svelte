<script lang="ts">
    // SimConfigPanel — Simulation configuration tab content.
    // All controls are UI-only in this phase.
    // TODO: wire to WASM params when config API is implemented.

    type RenderMode = "Points" | "Heat" | "Trails";
    type Challenge =
        | "Corners"
        | "Center"
        | "East edge"
        | "Right half"
        | "Pairs"
        | "Predator";

    let gridSize = $state(128);
    let population = $state(2048);
    let mutation = $state(0.001);
    let genomeLen = $state(16);
    let stepsPerGen = $state(300);
    let challenge = $state<Challenge>("Corners");
    let speed = $state(1);
    let renderMode = $state<RenderMode>("Points");
    let dirty = $state(false);

    const GRID_SIZES = [64, 128, 192, 256] as const;
    const CHALLENGES: Challenge[] = [
        "Corners",
        "Center",
        "East edge",
        "Right half",
        "Pairs",
        "Predator",
    ];
    const RENDER_MODES: RenderMode[] = ["Points", "Heat", "Trails"];
    const PRESETS = [
        "default",
        "corners-128",
        "predator-256",
        "sparse",
    ] as const;
</script>

<div class="sim-config">
    <!-- Panel header -->
    <div class="sim-config__header">
        <p class="small-caps sim-config__eyebrow">Configuration</p>
        <h2 class="sim-config__title">Simulation</h2>
        <p class="sim-config__subtitle">biosim4-gpu / OpenCL stepper</p>
    </div>

    <!-- Grid section -->
    <div class="section-label">
        <span class="small-caps">Grid</span>
        <span class="sim-config__hint">grid.h</span>
    </div>

    <div class="field-row">
        <div class="sim-config__field-header">
            <span class="field-label">Size</span>
            <span class="field-hint">{gridSize} × {gridSize} cells</span>
        </div>
        <div class="sim-config__pills">
            {#each GRID_SIZES as s}
                <button
                    class="button button--pill {gridSize === s
                        ? 'button--filled'
                        : 'button--ghost'}"
                    onclick={() => {
                        gridSize = s;
                        dirty = true;
                    }}
                    aria-pressed={gridSize === s}>{s}</button
                >
            {/each}
        </div>
    </div>

    <div class="field-row">
        <div class="sim-config__field-header">
            <span class="field-label">Seed</span>
            <span class="field-hint">hex / int</span>
        </div>
        <input
            class="control"
            type="text"
            value="0xA17C"
            aria-label="Random seed"
        />
    </div>

    <div class="sim-config__spacer"></div>

    <!-- Population section -->
    <div class="section-label">
        <span class="small-caps">Population</span>
        <span class="sim-config__hint">peeps.h</span>
    </div>

    <div class="field-row">
        <div class="sim-config__field-header">
            <span class="field-label">Initial population</span>
            <span class="field-hint"
                >{population.toLocaleString("fr-FR")} agents</span
            >
        </div>
        <div class="sim-config__slider-row">
            <input
                type="range"
                class="sim-config__slider"
                min="128"
                max="8192"
                step="64"
                value={population}
                oninput={(e) => {
                    population = parseFloat(
                        (e.target as HTMLInputElement).value,
                    );
                    dirty = true;
                }}
                aria-label="Initial population"
            />
            <span class="sim-config__slider-val">
                {population.toLocaleString("fr-FR")}
            </span>
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
                value={mutation}
                oninput={(e) => {
                    mutation = parseFloat((e.target as HTMLInputElement).value);
                    dirty = true;
                }}
                aria-label="Mutation rate"
            />
            <span class="sim-config__slider-val">{mutation.toFixed(4)}</span>
        </div>
    </div>

    <div class="field-row">
        <div class="sim-config__field-header">
            <span class="field-label">Genome length</span>
            <span class="field-hint">genes / agent</span>
        </div>
        <div class="sim-config__slider-row">
            <input
                type="range"
                class="sim-config__slider"
                min="4"
                max="32"
                step="1"
                value={genomeLen}
                oninput={(e) => {
                    genomeLen = parseFloat(
                        (e.target as HTMLInputElement).value,
                    );
                    dirty = true;
                }}
                aria-label="Genome length"
            />
            <span class="sim-config__slider-val">{genomeLen}</span>
        </div>
    </div>

    <div class="sim-config__spacer"></div>

    <!-- Evolution section -->
    <div class="section-label">
        <span class="small-caps">Evolution</span>
        <span class="sim-config__hint">generations.c</span>
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
                value={stepsPerGen}
                oninput={(e) => {
                    stepsPerGen = parseFloat(
                        (e.target as HTMLInputElement).value,
                    );
                    dirty = true;
                }}
                aria-label="Steps per generation"
            />
            <span class="sim-config__slider-val">{stepsPerGen}</span>
        </div>
    </div>

    <div class="field-row">
        <span class="field-label">Survival challenge</span>
        <div class="sim-config__pills sim-config__pills--wrap">
            {#each CHALLENGES as c}
                <button
                    class="button button--pill {challenge === c
                        ? 'button--filled'
                        : 'button--ghost'}"
                    onclick={() => {
                        challenge = c;
                        dirty = true;
                    }}
                    aria-pressed={challenge === c}>{c}</button
                >
            {/each}
        </div>
    </div>

    <div class="sim-config__spacer"></div>

    <!-- Runtime section -->
    <div class="section-label">
        <span class="small-caps">Runtime</span>
        <span class="sim-config__hint">runtime</span>
    </div>

    <div class="field-row">
        <div class="sim-config__field-header">
            <span class="field-label">Speed</span>
            <span class="field-hint">{speed}× realtime</span>
        </div>
        <div class="sim-config__slider-row">
            <input
                type="range"
                class="sim-config__slider"
                min="0.25"
                max="8"
                step="0.25"
                value={speed}
                oninput={(e) => {
                    speed = parseFloat((e.target as HTMLInputElement).value);
                    dirty = true;
                }}
                aria-label="Simulation speed"
            />
            <span class="sim-config__slider-val">{speed}×</span>
        </div>
    </div>

    <div class="field-row">
        <span class="field-label">Render mode</span>
        <div class="sim-config__pills">
            {#each RENDER_MODES as m}
                <button
                    class="button button--pill {renderMode === m
                        ? 'button--filled'
                        : 'button--ghost'}"
                    onclick={() => {
                        renderMode = m;
                        dirty = true;
                    }}
                    aria-pressed={renderMode === m}>{m}</button
                >
            {/each}
        </div>
    </div>

    <div class="sim-config__spacer"></div>

    <!-- Presets section -->
    <div class="section-label">
        <span class="small-caps">Presets</span>
        <span class="sim-config__hint">4 saved</span>
    </div>
    <div class="sim-config__pills sim-config__pills--wrap sim-config__presets">
        {#each PRESETS as p, i}
            <button
                class="button button--pill {i === 0
                    ? 'button--filled'
                    : 'button--ghost'}">{p}</button
            >
        {/each}
        <button class="button button--pill button--ghost"
            >＋ save current</button
        >
    </div>

    <div class="sim-config__spacer sim-config__spacer--lg"></div>
</div>

<!-- Sticky apply bar — stays at bottom of rail scroll area -->
<div class="sim-config__apply">
    <button
        class="button button--pill {dirty
            ? 'button--filled'
            : 'button--ghost'} sim-config__apply-btn"
        disabled={!dirty}
        onclick={() => {
            dirty = false;
        }}
        aria-label="Apply configuration and restart simulation"
    >
        {dirty ? "apply & restart →" : "✓ in sync"}
    </button>
    <button
        class="button button--pill button--ghost"
        aria-label="Export configuration as INI"
    >
        export .ini
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
        margin-bottom: var(--space-6);
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
