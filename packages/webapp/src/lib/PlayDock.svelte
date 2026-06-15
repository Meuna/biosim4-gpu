<script lang="ts">
    import {
        Play,
        Pause,
        StepForward,
        History,
        Baby,
        Dna,
        RefreshCw,
        Flag,
    } from "lucide-svelte";
    import ConfirmInline from "./ConfirmInline.svelte";
    import DiscreteSlider from "./DiscreteSlider.svelte";
    import type { SimPhase } from "./simMachine.svelte";

    const SPEED_STOPS: { value: number; label: string; ariaLabel: string }[] = [
        { value: 5, label: "5", ariaLabel: "5 fps" },
        { value: 25, label: "25", ariaLabel: "25 fps" },
        { value: 50, label: "50", ariaLabel: "50 fps" },
        { value: 0, label: ">>", ariaLabel: "Unlimited" },
    ];

    let {
        phase,
        genomIncompatible = false,
        targetSpeed,
        onToggle,
        onStep,
        onNextGen,
        onRewind,
        onClearGenom,
        onSetSpeed,
        onToggleFreeRun,
    }: {
        phase: SimPhase;
        genomIncompatible?: boolean;
        targetSpeed: number;
        onToggle: () => void;
        onStep: () => void;
        onNextGen: (autoPlay: boolean) => void;
        onRewind: (autoPlay: boolean) => void;
        onClearGenom: () => void;
        onSetSpeed: (fps: number) => void;
        onToggleFreeRun: () => void;
    } = $props();

    const isRunning = $derived(phase === "STEPS_RUNNING");
    const isFreeRunning = $derived(
        phase === "FREE_RUNNING" || phase === "FREE_RUN_STOPPING",
    );
    const isFreeRunStopping = $derived(phase === "FREE_RUN_STOPPING");
    const isGenComplete = $derived(phase === "GENERATION_ENDED");

    let clearConfirmOpen = $state(false);
</script>

<!-- display:contents — the two groups below participate directly in TopBar's
     grid; their grid-area names ("playback"/"evolution") are matched by the
     grid-template-areas in TopBar.svelte. -->
<div class="dock">
    <div class="dock__group dock__group--playback">
        <div class="dock__speed">
            <span class="dock__speed-label" aria-hidden="true">fps</span>
            <DiscreteSlider
                stops={SPEED_STOPS}
                value={targetSpeed}
                onChange={onSetSpeed}
                ariaLabel="Simulation speed"
            />
        </div>

        <div class="divider-v dock__sep" aria-hidden="true"></div>

        <button
            class="dock__btn dock__btn--primary"
            class:dock__btn--spent={isGenComplete}
            disabled={!isRunning &&
                (isGenComplete || genomIncompatible || isFreeRunning)}
            onclick={onToggle}
            aria-label={isRunning ? "Stop simulation" : "Play simulation"}
        >
            {#if isRunning}
                <Pause size={14} />
                Stop
            {:else if isGenComplete}
                <Flag size={14} />
                End
            {:else}
                <Play size={14} />
                Play
            {/if}
        </button>

        <button
            class="dock__btn"
            disabled={isRunning || isFreeRunning || genomIncompatible}
            onclick={onStep}
            aria-label="Step one simulation tick"
        >
            <StepForward size={14} />
            Step
        </button>
    </div>

    <div class="dock__group dock__group--evolution">
        <div class="dock__autoplay-group">
            <button
                class="dock__btn"
                class:dock__btn--prompt-breathe={isGenComplete}
                disabled={genomIncompatible || isFreeRunning}
                onclick={(e) => onNextGen(e.ctrlKey)}
                aria-label="Advance one generation (Ctrl+click to auto play)"
            >
                <Baby size={14} />
                Next Gen
            </button>

            <button
                class="dock__btn"
                class:dock__btn--prompt-breathe={isGenComplete}
                disabled={genomIncompatible || isFreeRunning}
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
                disabled={genomIncompatible || isFreeRunning}
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
                disabled={genomIncompatible || isFreeRunning}
                onclick={() => {
                    clearConfirmOpen = true;
                }}
                aria-label="Clear genome"
            >
                <Dna size={14} />
                Clear Genom
            </button>
        {/if}

        <div class="divider-v dock__sep" aria-hidden="true"></div>

        <button
            class="dock__btn dock__btn--evolve"
            class:dock__btn--evolve-active={isFreeRunning}
            disabled={isRunning || isFreeRunStopping || genomIncompatible}
            onclick={onToggleFreeRun}
            aria-label={isFreeRunning
                ? "Stop evolving"
                : "Evolve: auto-advance generations"}
        >
            <span class="evolve-icon"><RefreshCw size={14} /></span>
            Evolve
        </button>
    </div>
</div>

<style>
    /* display:contents lets the two groups below participate directly in
       TopBar's grid; .dock itself contributes no box. */
    .dock {
        display: contents;
    }

    .dock__group {
        display: flex;
        align-items: center;
        /* Centres the group when it spans a full-width wrapped row; a no-op in
           the auto-sized desktop tracks. */
        justify-self: center;
    }

    /* Area names are matched by grid-template-areas in TopBar.svelte. */
    .dock__group--playback {
        grid-area: playback;
    }

    .dock__group--evolution {
        grid-area: evolution;
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
        font-size: var(--text-xs);
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

    /* The generation is spent: a hollow, low-contrast pill of the SAME
       footprint as the filled primary (no border there, so a subtle border
       here keeps the size identical). Distinct from the dim filled look the
       incompatible-config case keeps via the default :disabled opacity. */
    .dock__btn--spent,
    .dock__btn--spent:disabled {
        background: transparent;
        color: var(--color-text-muted);
        border: 1px solid var(--color-border-subtle);
        opacity: 1;
    }

    /* Visuals come from .divider-v (primitives.css); only the intra-group
       spacing is component-specific. */
    .dock__sep {
        margin: 0 var(--space-3);
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
        font-size: var(--text-2xs);
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
        font-size: var(--text-2xs);
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

    @keyframes breathe {
        0%,
        100% {
            opacity: 0.5;
        }
        50% {
            opacity: 1;
        }
    }

    .dock__btn--prompt-breathe {
        color: var(--color-accent-text);
        animation: breathe 1.8s ease-in-out infinite;
    }

    @media (prefers-reduced-motion: reduce) {
        .dock__btn--prompt-breathe {
            animation: none;
        }
    }
</style>
