<script lang="ts">
    import {
        Play,
        Pause,
        StepForward,
        History,
        Baby,
        Dna,
        RefreshCw,
    } from "lucide-svelte";
    import ConfirmInline from "./ConfirmInline.svelte";
    import DiscreteSlider from "./DiscreteSlider.svelte";

    const SPEED_STOPS: { value: number; label: string; ariaLabel: string }[] = [
        { value: 5, label: "5", ariaLabel: "5 fps" },
        { value: 25, label: "25", ariaLabel: "25 fps" },
        { value: 50, label: "50", ariaLabel: "50 fps" },
        { value: 0, label: ">>", ariaLabel: "Unlimited" },
    ];

    let {
        running,
        genComplete,
        genomIncompatible = false,
        freeRunning,
        freeRunStopping,
        targetSpeed,
        onToggle,
        onStep,
        onNextGen,
        onRewind,
        onClearGenom,
        onSetSpeed,
        onToggleFreeRun,
    }: {
        running: boolean;
        genComplete: boolean;
        genomIncompatible?: boolean;
        freeRunning: boolean;
        freeRunStopping: boolean;
        targetSpeed: number;
        onToggle: () => void;
        onStep: () => void;
        onNextGen: (autoPlay: boolean) => void;
        onRewind: (autoPlay: boolean) => void;
        onClearGenom: () => void;
        onSetSpeed: (fps: number) => void;
        onToggleFreeRun: () => void;
    } = $props();

    let clearConfirmOpen = $state(false);
</script>

<div class="dock">
    <div class="dock__speed">
        <span class="dock__speed-label" aria-hidden="true">fps</span>
        <DiscreteSlider
            stops={SPEED_STOPS}
            value={targetSpeed}
            onChange={onSetSpeed}
            ariaLabel="Simulation speed"
        />
    </div>

    <div class="dock__sep" aria-hidden="true"></div>

    <button
        class="dock__btn dock__btn--primary"
        disabled={!running && (genComplete || genomIncompatible || freeRunning)}
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

    <button
        class="dock__btn"
        disabled={running || freeRunning}
        onclick={onStep}
        aria-label="Step one simulation tick"
    >
        <StepForward size={14} />
        Step
    </button>

    <div class="dock__sep" aria-hidden="true"></div>

    <div class="dock__autoplay-group">
        <button
            class="dock__btn"
            disabled={genomIncompatible || freeRunning}
            onclick={(e) => onNextGen(e.ctrlKey)}
            aria-label="Advance one generation (Ctrl+click to auto play)"
        >
            <Baby size={14} />
            Next Gen
        </button>

        <button
            class="dock__btn"
            disabled={genomIncompatible || freeRunning}
            onclick={(e) => onRewind(e.ctrlKey)}
            aria-label="Rewind: reproduce from last survivors (Ctrl+click to auto play)"
        >
            <History size={14} />
            Rewind
        </button>

        <span class="dock__autoplay-hint" aria-hidden="true"
            >ctrl+click to auto-play</span
        >
    </div>

    {#if clearConfirmOpen}
        <ConfirmInline
            open={clearConfirmOpen}
            confirmLabel="Clear"
            cancelLabel="Cancel"
            onConfirm={() => {
                clearConfirmOpen = false;
                onClearGenom();
            }}
            onCancel={() => {
                clearConfirmOpen = false;
            }}
        />
    {:else}
        <button
            class="dock__btn"
            onclick={() => {
                clearConfirmOpen = true;
            }}
            aria-label="Clear genome"
        >
            <Dna size={14} />
            Clear Genom
        </button>
    {/if}

    <div class="dock__sep" aria-hidden="true"></div>

    <button
        class="dock__btn dock__btn--evolve"
        class:dock__btn--evolve-active={freeRunning}
        disabled={running || freeRunStopping}
        onclick={onToggleFreeRun}
        aria-label={freeRunning
            ? "Stop evolving"
            : "Evolve: auto-advance generations"}
    >
        <span class="evolve-icon"><RefreshCw size={14} /></span>
        Evolve
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

    .dock__btn:not(.dock__btn--primary):hover:not(:disabled) {
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
        margin: 0 var(--space-3);
        font-weight: 500;
    }

    .dock__btn--primary:hover:not(:disabled) {
        background: var(--color-text-muted);
        opacity: 1;
    }

    .dock__sep {
        width: 1px;
        height: 1.25rem;
        background: var(--color-border-subtle);
        margin: 0 var(--space-3);
        flex-shrink: 0;
    }

    /* Shared hint floats below Next Gen + Rewind without affecting their height */
    .dock__autoplay-group {
        position: relative;
        display: flex;
        align-items: center;
    }

    .dock__autoplay-hint {
        position: absolute;
        top: 100%;
        left: 50%;
        transform: translateX(-50%);
        font-family: var(--font-mono);
        font-size: 0.5rem;
        color: var(--color-text-muted);
        opacity: 0;
        white-space: nowrap;
        pointer-events: none;
        letter-spacing: 0.03em;
        transition: opacity 0.15s;
    }

    .dock__autoplay-group:hover .dock__autoplay-hint {
        opacity: 0.55;
    }

    .dock__speed {
        display: inline-flex;
        align-items: center;
        gap: var(--space-1);
        margin-right: var(--space-3);
    }

    .dock__speed-label {
        font-family: var(--font-mono);
        font-size: 0.625rem;
        color: var(--color-text-muted);
        letter-spacing: 0.04em;
        padding-left: var(--space-2);
        white-space: nowrap;
    }

    @keyframes spin {
        from {
            transform: rotate(0deg);
        }
        to {
            transform: rotate(360deg);
        }
    }

    .dock__btn--evolve:hover:not(:disabled) .evolve-icon,
    .dock__btn--evolve-active .evolve-icon {
        animation: spin 1.2s linear infinite;
    }

    .dock__btn--evolve-active {
        border: 1px solid var(--color-border-subtle);
        border-radius: var(--radius-sm);
    }
</style>
