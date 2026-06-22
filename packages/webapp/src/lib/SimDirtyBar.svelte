<script lang="ts">
    // SimDirtyBar — a dirty-state action bar that slides open beneath the
    // Simulation rail tab (outside the scrollable body, so it never scrolls
    // away). It pairs the rewind (History) way to commit a dirty draft with
    // "revert" (Undo2) to discard it.
    import { slide } from "svelte/transition";
    import { Undo2, History } from "lucide-svelte";

    let {
        dirty,
        disabled,
        onRevert,
        onRewind,
    }: {
        dirty: boolean;
        disabled: boolean;
        onRevert: () => void;
        onRewind: (autoPlay: boolean) => void;
    } = $props();

    // Collapse the open/close animation to an instant toggle when the user
    // prefers reduced motion. matchMedia is absent under jsdom, hence the guard.
    const reduceMotion =
        typeof window !== "undefined" &&
        (window.matchMedia?.("(prefers-reduced-motion: reduce)").matches ??
            false);
    const slideDuration = reduceMotion ? 0 : 200;
</script>

{#if dirty}
    <div
        class="dirty-bar"
        role="group"
        aria-label="Pending configuration changes"
        transition:slide={{ duration: slideDuration }}
    >
        <div class="dirty-bar__group">
            <button
                class="button button--utility"
                {disabled}
                onclick={(e) => onRewind(e.ctrlKey)}
                aria-label="Rewind: reproduce from last survivors (Ctrl+click to auto play)"
            >
                <span class="dirty-bar__label" aria-hidden="true">save</span>
                <History size={14} />
            </button>
        </div>

        <div class="divider-v" aria-hidden="true"></div>

        <div class="dirty-bar__group">
            <button
                class="button button--utility"
                onclick={onRevert}
                aria-label="Revert all changes"
            >
                <span class="dirty-bar__label" aria-hidden="true">revert</span>
                <Undo2 size={14} />
            </button>
        </div>
    </div>
{/if}

<style>
    .dirty-bar {
        display: flex;
        align-items: center;
        justify-content: center;
        gap: var(--space-3);
        padding: var(--space-2) var(--space-4);
        border-bottom: 1px solid var(--color-border-subtle);
        background: var(--color-surface-alt);
        flex-shrink: 0;
    }

    .dirty-bar__group {
        display: flex;
        align-items: center;
        gap: var(--space-1);
    }

    .dirty-bar__label {
        font-family: var(--font-mono);
        font-size: var(--text-xs);
        text-transform: uppercase;
        letter-spacing: var(--tracking-wider);
        color: var(--color-text-muted);
    }
</style>
