<script lang="ts">
    // PresetSelector — the simulation-preset picker that replaces the static
    // panel title. The small-caps eyebrow reads "Preset"; the title is a button
    // showing the selected preset's name with a chevron that rotates when the
    // list is open (the affordance that reveals the title is clickable).
    // Clicking opens a dropdown of presets; entries that carry a survivor
    // snapshot render a small DNA glyph. Closes on selection, Escape, and an
    // outside click.
    import { ChevronDown, Dna } from "lucide-svelte";
    import type { Preset } from "./presets";

    interface Props {
        presets: Preset[];
        selectedId: string;
        disabled?: boolean;
        onSelect: (preset: Preset) => void;
    }
    const { presets, selectedId, disabled = false, onSelect }: Props = $props();

    let open = $state(false);
    let rootEl: HTMLDivElement | undefined;

    const selectedName = $derived(
        presets.find((p) => p.id === selectedId)?.name ?? "Default",
    );

    function choose(preset: Preset): void {
        open = false;
        onSelect(preset);
    }

    // While open, close on Escape or a click outside the selector. The effect
    // only attaches/detaches listeners; it never writes the state it reads, so
    // it cannot self-trigger.
    $effect(() => {
        if (!open) return;
        function onKey(e: KeyboardEvent): void {
            if (e.key === "Escape") open = false;
        }
        function onClick(e: MouseEvent): void {
            if (rootEl && !rootEl.contains(e.target as Node)) open = false;
        }
        window.addEventListener("keydown", onKey);
        window.addEventListener("click", onClick, true);
        return () => {
            window.removeEventListener("keydown", onKey);
            window.removeEventListener("click", onClick, true);
        };
    });
</script>

<div class="preset" bind:this={rootEl}>
    <button
        class="preset__title"
        {disabled}
        onclick={() => (open = !open)}
        aria-haspopup="listbox"
        aria-expanded={open}
    >
        <span class="preset__name">{selectedName}</span>
        <span class="preset__chevron" class:preset__chevron--open={open}>
            <ChevronDown size={18} />
        </span>
    </button>

    {#if open}
        <ul class="preset__menu" role="listbox" aria-label="Simulation preset">
            {#each presets as p (p.id)}
                <li role="option" aria-selected={p.id === selectedId}>
                    <button
                        class="preset__option"
                        class:preset__option--active={p.id === selectedId}
                        onclick={() => choose(p)}
                    >
                        <span class="preset__option-name">{p.name}</span>
                        {#if p.hasSnapshot}
                            <Dna size={14} class="icon-inline" />
                        {/if}
                    </button>
                </li>
            {/each}
        </ul>
    {/if}
</div>

<style>
    .preset {
        position: relative;
    }

    .preset__title {
        display: inline-flex;
        align-items: center;
        gap: var(--space-2);
        padding: 0 0 var(--space-2);
        border: 0;
        background: none;
        cursor: pointer;
        color: var(--color-text);
        font-family: var(--font-sans);
        font-size: var(--text-2xl);
        font-weight: 700;
        line-height: 1.12;
    }

    .preset__title:disabled {
        cursor: default;
    }

    .preset__name {
        border-bottom: 2px dashed var(--color-border-subtle);
        transition: border-color 0.1s;
    }

    .preset__title:hover:not(:disabled) .preset__name,
    .preset__title:focus-visible .preset__name {
        border-bottom-color: var(--color-accent);
    }

    .preset__chevron {
        display: inline-flex;
        color: var(--color-text-muted);
        transition: transform 0.18s ease;
    }

    .preset__chevron--open {
        transform: rotate(180deg);
    }

    .preset__menu {
        position: absolute;
        z-index: 40;
        top: calc(100% + var(--space-2));
        left: 0;
        min-width: 14rem;
        margin: 0;
        padding: var(--space-1);
        list-style: none;
        background: var(--color-surface);
        border: 1px solid var(--color-border);
        border-radius: var(--radius-sm);
        box-shadow: var(--shadow-floating);
    }

    .preset__option {
        display: flex;
        align-items: center;
        justify-content: space-between;
        gap: var(--space-3);
        width: 100%;
        padding: var(--space-2) var(--space-3);
        border: 0;
        border-radius: var(--radius-sm);
        background: none;
        cursor: pointer;
        color: var(--color-text);
        font-family: var(--font-sans);
        font-size: var(--text-sm);
        text-align: left;
    }

    .preset__option:hover {
        background: var(--color-surface-alt);
    }

    .preset__option--active {
        color: var(--color-accent-text);
        font-weight: 700;
    }
</style>
