<script lang="ts">
    import PlayDock from "./PlayDock.svelte";
    import type { SimPhase } from "./simMachine.svelte";
    import { webappVersion, snapshotCompatNoteText } from "./version";

    let {
        phase,
        genomIncompatible = false,
        targetSpeed,
        headerHeight = $bindable(56),
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
        headerHeight?: number;
        onToggle: () => void;
        onStep: () => void;
        onNextGen: (autoPlay: boolean) => void;
        onRewind: (autoPlay: boolean) => void;
        onClearGenom: () => void;
        onSetSpeed: (fps: number) => void;
        onToggleFreeRun: () => void;
    } = $props();
</script>

<header class="topbar" bind:offsetHeight={headerHeight}>
    <div class="topbar__left">
        <span class="topbar__brand">biosim4-gpu</span>
        <span class="topbar__subtitle small-caps">visualizer</span>
    </div>

    <PlayDock
        {phase}
        {genomIncompatible}
        {targetSpeed}
        {onToggle}
        {onStep}
        {onNextGen}
        {onRewind}
        {onClearGenom}
        {onSetSpeed}
        {onToggleFreeRun}
    />

    <!-- Separator between the playback and evolution groups. Only present in the
         1-row layout (≥1060px); hidden once the groups wrap to different rows. -->
    <div class="topbar__sep divider-v" aria-hidden="true"></div>

    <div class="topbar__right">
        <a
            href="https://github.com/meuna/biosim4-gpu"
            target="_blank"
            rel="noopener noreferrer"
            class="topbar__github"
        >
            <svg
                width="16"
                height="16"
                viewBox="0 0 16 16"
                fill="currentColor"
                aria-hidden="true"
            >
                <path
                    d="M8 0C3.58 0 0 3.58 0 8c0 3.54 2.29 6.53 5.47 7.59.4.07.55-.17.55-.38 0-.19-.01-.82-.01-1.49-2.01.37-2.53-.49-2.69-.94-.09-.23-.48-.94-.82-1.13-.28-.15-.68-.52-.01-.53.63-.01 1.08.58 1.23.82.72 1.21 1.87.87 2.33.66.07-.52.28-.87.51-1.07-1.78-.2-3.64-.89-3.64-3.95 0-.87.31-1.59.82-2.15-.08-.2-.36-1.02.08-2.12 0 0 .67-.21 2.2.82.64-.18 1.32-.27 2-.27.68 0 1.36.09 2 .27 1.53-1.04 2.2-.82 2.2-.82.44 1.1.16 1.92.08 2.12.51.56.82 1.27.82 2.15 0 3.07-1.87 3.75-3.65 3.95.29.25.54.73.54 1.48 0 1.07-.01 1.93-.01 2.2 0 .21.15.46.55.38A8.013 8.013 0 0016 8c0-4.42-3.58-8-8-8z"
                />
            </svg>
            <span>GitHub</span>
        </a>
    </div>
</header>

<!-- Build version, parked just under the bar so it stays legible without
     crowding the brand/subtitle. The compat marker sits beside it. -->
<div class="topbar__version small-caps">
    <span>{webappVersion}</span>
    {#if snapshotCompatNoteText}
        <span
            class="topbar__compat"
            title={snapshotCompatNoteText}
            aria-label={snapshotCompatNoteText}>- ⚠ Incompatible snapshots</span
        >
    {/if}
</div>

<style>
    /* Responsive header grid. The bar reflows into 1 / 2 / 3 rows as the
       viewport narrows; min-height (not height) lets it grow when it wraps so
       the measured height published by App.svelte tracks the real height.
       Breakpoints 1160/760 are duplicated as literals in the @media rules below
       because var() cannot be used inside @media conditions. (1160 is where the
       full brand + the ~750px dock + github stop fitting on one line.) */
    .topbar {
        position: fixed;
        left: 0;
        right: 0;
        top: 0;
        min-height: 3.5rem;
        z-index: 20;
        display: grid;
        align-items: center;
        column-gap: var(--space-3);
        row-gap: var(--space-2);
        padding: var(--space-2) var(--space-6);
        background: var(--color-surface-glass);
        backdrop-filter: blur(6px);
        border-bottom: 1px solid var(--color-border-subtle);

        /* 1-row (≥1160px): the dock (playback·sep·evolution) is centred while
           there is symmetric room. The left track's min is the brand's
           max-content, so once the brand+subtitle get close the brand *pushes*
           the dock rightward instead of overlapping it (and the wasteful empty
           space beside the small github link is reclaimed). The right track only
           needs github's min-content, so it yields first. */
        grid-template-columns:
            minmax(max-content, 1fr) auto auto auto
            minmax(min-content, 1fr);
        grid-template-areas: "brand playback sep evolution github";
    }

    /* <1160px: 2 rows (brand · playback · github / evolution). */
    @media (max-width: 1159.98px) {
        .topbar {
            grid-template-columns: 1fr auto 1fr;
            grid-template-areas:
                "brand playback github"
                "evolution evolution evolution";
        }

        /* The sep's area is absent from this template; it must be removed from
           flow, not merely unreferenced, or it auto-places and breaks layout. */
        .topbar__sep {
            display: none;
        }
    }

    /* <760px: 3 rows (brand · github / playback / evolution). */
    @media (max-width: 759.98px) {
        .topbar {
            grid-template-columns: 1fr 1fr;
            grid-template-areas:
                "brand github"
                "playback playback"
                "evolution evolution";
        }
    }

    .topbar__left {
        grid-area: brand;
        justify-self: start;
        display: flex;
        align-items: baseline;
        /* On 1 row the brand track is minmax(max-content, 1fr), so this is given
           its full one-line width and never wraps — that max-content is also the
           boundary that pushes the dock right. On 2/3 rows the track is 1fr and
           can shrink: flex-wrap then lets the subtitle drop below the brand
           (which itself stays whole via .topbar__brand) so nothing clips down to
           ~320px. */
        flex-wrap: wrap;
        gap: var(--space-3);
    }

    .topbar__brand {
        font-family: var(--font-sans);
        font-size: 1.375rem;
        font-weight: 700;
        color: var(--color-text);
        letter-spacing: -0.02em;
        white-space: nowrap;
    }

    .topbar__compat {
        color: var(--color-warn);
        cursor: help;
    }

    .topbar__version {
        position: fixed;
        /* Follows the measured header height published by App.svelte; 3.5rem is
           the fallback that matches the bar's default (non-wrapped) height. */
        top: calc(var(--topbar-h, 3.5rem) + var(--space-3));
        left: var(--space-6);
        z-index: 20;
        display: flex;
        align-items: baseline;
        gap: var(--space-2);
    }

    /* Visuals come from .divider-v (primitives.css); margin matches the
       intra-group seps so the 1-row dock reads as a single ruled strip. */
    .topbar__sep {
        grid-area: sep;
        margin: 0 var(--space-3);
    }

    .topbar__right {
        grid-area: github;
        justify-self: end;
        display: flex;
        align-items: center;
    }

    .topbar__github {
        display: inline-flex;
        align-items: center;
        gap: var(--space-2);
        font-family: var(--font-mono);
        font-size: var(--text-xs);
        color: var(--color-text-muted);
        text-decoration: none;
        letter-spacing: 0.06em;
        transition: color 0.1s;
    }

    .topbar__github:hover {
        color: var(--color-text);
    }
</style>
