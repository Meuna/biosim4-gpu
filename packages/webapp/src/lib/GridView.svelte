<script lang="ts">
    import type { SimState } from "./simState";

    let {
        geom,
        simState,
        gridSizeX = 128,
        gridSizeY = 128,
        blurred = false,
    }: {
        geom: {
            x: number;
            y: number;
            w: number;
            h: number;
            cx: number;
            cy: number;
        };
        simState: SimState;
        gridSizeX?: number;
        gridSizeY?: number;
        blurred?: boolean;
    } = $props();
</script>

<!--
    GridView — transparent DOM overlay for the grid zone.
    NO canvas inside this component. The full-viewport canvas lives in App.svelte
    at z-index 0; this overlay sits above it at z-index 5.
    Provides: 4 crop-mark corners, axis labels, and the idle "press play" overlay.
-->
<div
    class="grid-view"
    class:grid-view--blurred={blurred}
    style="left: {geom.x}px; top: {geom.y}px; width: {geom.w}px; height: {geom.h}px"
    aria-label="Simulation grid zone"
>
    <!-- Crop marks — 4 corners just outside the zone boundary -->
    <div class="crop crop--tl" aria-hidden="true"></div>
    <div class="crop crop--tr" aria-hidden="true"></div>
    <div class="crop crop--bl" aria-hidden="true"></div>
    <div class="crop crop--br" aria-hidden="true"></div>

    <!-- Axis labels -->
    <span class="axis-label axis-label--origin" aria-hidden="true">0</span>
    <span class="axis-label axis-label--y-end" aria-hidden="true"
        >{gridSizeY}</span
    >
    <span class="axis-label axis-label--x-end" aria-hidden="true"
        >{gridSizeX}</span
    >

    <!-- Idle overlay — visible only before simulation has ever been started -->
    {#if simState === "WORKER_PENDING" || simState === "WORKER_READY"}
        <div class="idle-overlay" aria-label="Simulation not started">
            <p class="idle-overlay__hint small-caps">Awaiting initialization</p>
            <p class="idle-overlay__display">
                press play<br /><em>to begin.</em>
            </p>
            <p class="idle-overlay__meta">
                {gridSizeX} × {gridSizeY} · seed 0xA17C · 2 048 cells queued
            </p>
        </div>
    {/if}
    <!-- TODO: replace idle overlay with actual agent rendering on the canvas layer -->
</div>

<style>
    .grid-view {
        position: absolute;
        z-index: 5;
        pointer-events: none;
        transition:
            filter 0.2s,
            opacity 0.2s;
    }

    .grid-view--blurred {
        filter: blur(2px);
        opacity: 0.5;
    }

    /* ── Crop marks ── */
    .crop {
        position: absolute;
        width: 1.125rem;
        height: 1.125rem;
        border-color: var(--color-text);
        border-style: solid;
        border-width: 0;
    }

    .crop--tl {
        left: -2px;
        top: -2px;
        border-top-width: 1.5px;
        border-left-width: 1.5px;
    }

    .crop--tr {
        right: -2px;
        top: -2px;
        border-top-width: 1.5px;
        border-right-width: 1.5px;
    }

    .crop--bl {
        left: -2px;
        bottom: -2px;
        border-bottom-width: 1.5px;
        border-left-width: 1.5px;
    }

    .crop--br {
        right: -2px;
        bottom: -2px;
        border-bottom-width: 1.5px;
        border-right-width: 1.5px;
    }

    /* ── Axis labels ── */
    .axis-label {
        position: absolute;
        font-family: var(--font-mono);
        font-size: 9px;
        color: var(--color-text-muted);
        line-height: 1;
    }

    .axis-label--origin {
        left: -1.5rem;
        top: -2px;
    }

    .axis-label--y-end {
        left: -2.25rem;
        bottom: -2px;
    }

    .axis-label--x-end {
        right: -2px;
        bottom: -1.25rem;
    }

    /* ── Idle overlay ── */
    .idle-overlay {
        position: absolute;
        inset: 0;
        display: flex;
        flex-direction: column;
        align-items: center;
        justify-content: center;
        gap: var(--space-1);
        text-align: center;
    }

    .idle-overlay__hint {
        margin-bottom: var(--space-3);
        /* .small-caps class handles font/size/casing */
    }

    .idle-overlay__display {
        font-family: var(--font-sans);
        font-size: var(--text-4xl);
        font-weight: 700;
        line-height: 1.05;
        color: var(--color-text);
        margin: 0;
    }

    .idle-overlay__display em {
        font-style: italic;
    }

    .idle-overlay__meta {
        font-family: var(--font-mono);
        font-size: 11px;
        color: var(--color-text-muted);
        margin-top: var(--space-5);
        margin-bottom: 0;
    }
</style>
