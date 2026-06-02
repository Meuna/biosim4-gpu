<script lang="ts">
    import {
        Play,
        Pause,
        StepForward,
        History,
        Baby,
        Dna,
    } from "lucide-svelte";

    let {
        running,
        onToggle,
        onStep,
        onGen,
        onRewind,
    }: {
        running: boolean;
        onToggle: () => void;
        onStep: () => void;
        onGen: () => void;
        onRewind: () => void;
    } = $props();
</script>

<div class="dock">
    <button
        class="dock__btn dock__btn--primary"
        onclick={onToggle}
        aria-label={running ? "Stop simulation" : "Play simulation"}
    >
        {#if running}
            <Pause size={14} />
            Stop
        {:else}
            <Play size={14} />
            Play
        {/if}
    </button>

    <div class="dock__sep" aria-hidden="true"></div>

    <button
        class="dock__btn"
        onclick={onStep}
        aria-label="Step one simulation tick"
    >
        <StepForward size={14} />
        Step
    </button>

    <div class="dock__sep" aria-hidden="true"></div>

    <button
        class="dock__btn"
        onclick={onGen}
        aria-label="Advance one generation"
    >
        <Baby size={14} />
        Next Gen
    </button>

    <button
        class="dock__btn"
        onclick={onRewind}
        aria-label="Rewind: reproduce a new generation from the last survivors"
    >
        <History size={14} />
        Rewind
    </button>

    <button
        class="dock__btn"
        disabled
        aria-label="Clear genome (not yet implemented)"
    >
        <Dna size={14} />
        Clear Genom
    </button>
</div>

<style>
    .dock {
        display: flex;
        align-items: center;
        gap: 0;
    }

    .dock__btn {
        display: inline-flex;
        align-items: center;
        gap: var(--space-1);
        height: 2rem;
        padding: 0 var(--space-2);
        background: transparent;
        border: none;
        border-radius: var(--radius-sm);
        cursor: pointer;
        font-family: var(--font-mono);
        font-size: 0.75rem;
        color: var(--color-text-muted);
        letter-spacing: 0.04em;
        transition:
            color 0.1s,
            background 0.1s;
        white-space: nowrap;
    }

    .dock__btn:hover:not(:disabled) {
        color: var(--color-text);
    }

    .dock__btn:focus-visible {
        outline: 2px solid var(--color-accent);
        outline-offset: 2px;
    }

    .dock__btn:disabled {
        opacity: 0.35;
        cursor: default;
    }

    .dock__btn--primary {
        background: var(--color-text);
        color: var(--color-surface);
        border-radius: var(--radius-pill);
        padding: 0 var(--space-3);
        font-weight: 500;
    }

    .dock__btn--primary:hover {
        opacity: 0.85;
        color: var(--color-surface);
    }

    .dock__sep {
        width: 1px;
        height: 1.25rem;
        background: var(--color-border-subtle);
        margin: 0 var(--space-2);
        flex-shrink: 0;
    }
</style>
