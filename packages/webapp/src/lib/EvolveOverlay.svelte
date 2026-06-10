<script lang="ts">
    import type { SimPhase } from "./simMachine.svelte";

    let {
        geom,
        gen,
        phase,
    }: {
        geom: { x: number; y: number; w: number; h: number };
        gen: number;
        phase: SimPhase;
    } = $props();

    const stopping = $derived(phase === "FREE_RUN_STOPPING");
</script>

<div
    class="evolve-overlay"
    style="left: {geom.x}px; top: {geom.y}px; width: {geom.w}px; height: {geom.h}px"
    role="status"
    aria-live="polite"
>
    <span class="evolve-overlay__label small-caps">Evolving</span>
    <span class="evolve-overlay__gen">Gen: {gen}</span>
    {#if stopping}
        <span class="evolve-overlay__stop">Stop request sent…</span>
    {/if}
</div>

<style>
    .evolve-overlay {
        position: fixed;
        z-index: 10;
        display: flex;
        flex-direction: column;
        align-items: center;
        justify-content: center;
        gap: var(--space-3);
    }

    .evolve-overlay__label {
        /* .small-caps provides font/size/spacing/color */
    }

    .evolve-overlay__gen {
        font-family: var(--font-mono);
        font-size: 4rem;
        font-weight: 700;
        color: var(--color-text);
        letter-spacing: -0.02em;
        line-height: 1;
    }

    .evolve-overlay__stop {
        font-family: var(--font-mono);
        font-size: 0.625rem;
        letter-spacing: 0.1em;
        color: var(--color-text-muted);
    }
</style>
