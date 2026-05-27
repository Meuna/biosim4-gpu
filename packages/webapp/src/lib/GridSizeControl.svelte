<script lang="ts">
    // GridSizeControl — grid dimension control with quick-pick presets and
    // independent X/Y sliders always visible side by side.
    // Clicking a preset sets both sliders to that square size (and marks it active).
    // Moving either slider independently deactivates any preset.
    // The section-label and field-row structural divs live in SimConfigPanel.
    import ParamSlider from "./ParamSlider.svelte";

    interface Props {
        gridSizeX: number;
        gridSizeY: number;
        onchange: (x: number, y: number) => void;
    }
    const { gridSizeX, gridSizeY, onchange }: Props = $props();

    const PRESETS = [64, 128, 192, 256] as const;

    function isPresetActive(s: number): boolean {
        return gridSizeX === s && gridSizeY === s;
    }
</script>

<!-- Preset pills — clicking one sets both dimensions to that square size -->
<div class="grid-size__pills">
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

<!-- Independent X/Y sliders — always visible; editing deactivates any preset -->
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

<style>
    .grid-size__pills {
        display: flex;
        gap: var(--space-3);
        margin-bottom: var(--space-2);
    }
</style>
