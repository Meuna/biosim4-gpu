<script lang="ts">
    // PresetSelector — the simulation-preset picker that replaces the static
    // panel title. A native <select> drives the choice (so handhelds get the
    // OS picker); it is styled to look like the big panel title, with the
    // browser arrow hidden and a custom chevron drawn at the right edge.
    // When the selected preset carries a survivor snapshot, a DNA glyph sits
    // between the name and the chevron.
    import { ChevronDown, Dna } from "lucide-svelte";
    import type { Preset } from "./presets";

    interface Props {
        presets: Preset[];
        selectedId: string;
        disabled?: boolean;
        onSelect: (preset: Preset) => void;
    }
    const { presets, selectedId, disabled = false, onSelect }: Props = $props();

    const selectedHasSnapshot = $derived(
        presets.find((p) => p.id === selectedId)?.hasSnapshot ?? false,
    );

    function choose(event: Event): void {
        const id = (event.currentTarget as HTMLSelectElement).value;
        const preset = presets.find((p) => p.id === id);
        if (preset) onSelect(preset);
    }

    // A native <select> fires no change event when the user re-picks the
    // already-selected option, so selecting a preset that is already current
    // would not re-apply it. Clear the value as the picker opens; any pick —
    // even the previously-selected id — then differs from "" and fires change.
    function openPicker(event: Event): void {
        (event.currentTarget as HTMLSelectElement).value = "";
    }

    // Restore the displayed value if the picker is dismissed without a pick, so
    // the blanked value never lingers as an empty title.
    function restore(event: Event): void {
        (event.currentTarget as HTMLSelectElement).value = selectedId;
    }

    // Escape closes the picker without firing change and (on desktop) without
    // firing blur, since focus stays on the select — restore the value here.
    function onKey(event: KeyboardEvent): void {
        if (event.key === "Escape") restore(event);
    }
</script>

<div class="preset" class:preset--snapshot={selectedHasSnapshot}>
    <select
        class="preset__select"
        name="simulation-preset"
        {disabled}
        value={selectedId}
        onchange={choose}
        onpointerdown={openPicker}
        onblur={restore}
        onkeydown={onKey}
        aria-label="Simulation preset"
    >
        {#each presets as p (p.id)}
            <option value={p.id}>{p.name}</option>
        {/each}
    </select>
    <span class="preset__adornment" aria-hidden="true">
        {#if selectedHasSnapshot}
            <Dna size={16} class="icon-inline preset__dna" />
        {/if}
        <span class="preset__chevron">
            <ChevronDown size={18} class="icon-inline" />
        </span>
    </span>
</div>

<style>
    .preset {
        position: relative;
        display: inline-flex;
        align-items: center;
    }

    .preset__select {
        appearance: none;
        padding: 0 calc(var(--space-2) + 18px) var(--space-2) 0;
        border: 0;
        border-bottom: 2px dashed var(--color-border-subtle);
        background: none;
        cursor: pointer;
        color: var(--color-text);
        font-family: var(--font-sans);
        font-size: var(--text-2xl);
        font-weight: 700;
        line-height: 1.12;
        transition: border-color 0.1s;
    }

    /* Widen the trailing gap so the DNA glyph fits before the chevron. */
    .preset--snapshot .preset__select {
        padding-right: calc(var(--space-2) + 16px + var(--space-2) + 18px);
    }

    .preset__select:disabled {
        cursor: default;
    }

    .preset__select:hover:not(:disabled),
    .preset__select:focus-visible {
        border-bottom-color: var(--color-accent);
        outline: none;
    }

    .preset__adornment {
        position: absolute;
        right: 0;
        bottom: var(--space-2);
        display: inline-flex;
        align-items: center;
        gap: var(--space-2);
        pointer-events: none;
    }

    .preset__chevron {
        display: inline-flex;
        color: var(--color-text-muted);
    }
</style>
