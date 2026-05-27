<script lang="ts">
    // GridSizeControl — per-parameter preset/custom toggle for grid dimensions.
    // Preset mode shows quick-pick pill buttons (square sizes).
    // Custom mode shows two ParamSlider components for independent X/Y control.
    import ParamSlider from "./ParamSlider.svelte";

    interface Props {
        gridSizeX: number;
        gridSizeY: number;
        onchange: (x: number, y: number) => void;
    }
    const { gridSizeX, gridSizeY, onchange }: Props = $props();

    const PRESETS = [64, 128, 192, 256] as const;
    let mode = $state<"preset" | "custom">("preset");

    function isPresetActive(s: number): boolean {
        return gridSizeX === s && gridSizeY === s;
    }
</script>

<div class="section-label">
    <span class="small-caps">Grid</span>
    <span class="sim-config__hint">grid.h</span>
</div>

<div class="field-row">
    <div class="grid-size__header">
        <span class="field-label">Size</span>
        <button
            class="button button--pill button--ghost grid-size__toggle"
            onclick={() => {
                mode = mode === "preset" ? "custom" : "preset";
            }}
        >
            {mode === "preset" ? "Custom →" : "← Preset"}
        </button>
    </div>

    {#if mode === "preset"}
        <div class="sim-config__pills">
            {#each PRESETS as s}
                <button
                    class="button button--pill {isPresetActive(s)
                        ? 'button--filled'
                        : 'button--ghost'}"
                    onclick={() => onchange(s, s)}
                    aria-pressed={isPresetActive(s)}
                >
                    {s}
                </button>
            {/each}
        </div>
    {:else}
        <ParamSlider
            label="Width (X)"
            min={16}
            max={512}
            step={1}
            value={gridSizeX}
            onchange={(v) => onchange(v, gridSizeY)}
        />
        <ParamSlider
            label="Height (Y)"
            min={16}
            max={512}
            step={1}
            value={gridSizeY}
            onchange={(v) => onchange(gridSizeX, v)}
        />
    {/if}
</div>

<style>
    .grid-size__header {
        display: flex;
        justify-content: space-between;
        align-items: center;
        margin-bottom: var(--space-2);
    }

    .grid-size__header .field-label {
        margin-bottom: 0;
    }

    .grid-size__toggle {
        font-size: var(--text-sm);
        padding: var(--space-1) var(--space-2);
    }
</style>
