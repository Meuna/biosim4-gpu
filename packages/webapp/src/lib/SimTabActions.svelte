<script lang="ts">
    // SimTabActions — glyph-only revert / next-gen / rewind controls surfaced in
    // the always-visible Simulation rail tab, so the dirty-config signal and the
    // commit actions stay visible even when the config panel is scrolled away.
    import { Undo2, Baby, History } from "lucide-svelte";

    let {
        dirty,
        disabled,
        onRevert,
        onNextGen,
        onRewind,
    }: {
        dirty: boolean;
        disabled: boolean;
        onRevert: () => void;
        onNextGen: (autoPlay: boolean) => void;
        onRewind: (autoPlay: boolean) => void;
    } = $props();
</script>

<div class="sim-tab-actions" role="group" aria-label="Simulation controls">
    <button
        class="button button--utility"
        class:sim-tab-actions__revert--dirty={dirty}
        disabled={!dirty}
        onclick={onRevert}
        aria-label="Revert all changes"
    >
        <Undo2 size={14} />
    </button>
    <button
        class="button button--utility"
        {disabled}
        onclick={(e) => onNextGen(e.ctrlKey)}
        aria-label="Advance one generation (Ctrl+click to auto play)"
    >
        <Baby size={14} />
    </button>
    <button
        class="button button--utility"
        {disabled}
        onclick={(e) => onRewind(e.ctrlKey)}
        aria-label="Rewind: reproduce from last survivors (Ctrl+click to auto play)"
    >
        <History size={14} />
    </button>
</div>

<style>
    .sim-tab-actions {
        display: flex;
        gap: var(--space-2);
    }

    /* When the draft diverges from the running config, the enabled revert glyph
       reads as a positive accent highlight rather than the mere absence of a
       disabled dim. */
    .sim-tab-actions__revert--dirty {
        color: var(--color-accent);
    }

    .sim-tab-actions__revert--dirty:hover {
        color: var(--color-accent-text);
    }
</style>
