<script lang="ts">
    import type { AgentInfo } from "../workers/sim.worker";
    import { HEADINGS } from "./headings";

    let {
        agent,
        x,
        y,
    }: {
        agent: AgentInfo | null;
        x: number;
        y: number;
    } = $props();

    // Offset from cursor; flip left/up near viewport edges.
    const OFFSET = 16;
    const CARD_W = 200;
    const CARD_H = 110;

    const left = $derived(
        x + OFFSET + CARD_W > window.innerWidth
            ? x - OFFSET - CARD_W
            : x + OFFSET,
    );
    const top = $derived(
        y + OFFSET + CARD_H > window.innerHeight
            ? y - OFFSET - CARD_H
            : y + OFFSET,
    );
</script>

{#if agent}
    <div
        class="hover-card"
        style="left: {left}px; top: {top}px;"
        aria-hidden="true"
    >
        <div class="hover-card__row hover-card__title">
            #{agent.id.toString().padStart(4, "0")}
        </div>
        <div class="hover-card__row">
            <span class="hover-card__key">x, y</span>
            <span class="hover-card__val">{agent.gx}, {agent.gy}</span>
        </div>
        <div class="hover-card__row">
            <span class="hover-card__key">dir</span>
            <span class="hover-card__val">{HEADINGS[agent.heading % 8]}</span>
        </div>
        <div class="hover-card__row">
            <span class="hover-card__key">gene</span>
            <span class="hover-card__val">0x{agent.fingerprint}</span>
        </div>
    </div>
{/if}

<style>
    .hover-card {
        position: fixed;
        z-index: 50;
        background: var(--color-surface-glass);
        border: 1px solid var(--color-border-subtle);
        border-radius: var(--radius-md);
        box-shadow: var(--shadow-floating);
        padding: var(--space-2) var(--space-3);
        pointer-events: none;
        backdrop-filter: blur(4px);
        min-width: 10rem;
    }

    .hover-card__title {
        font-family: var(--font-mono);
        font-size: var(--text-xs);
        font-weight: 700;
        color: var(--color-accent-text);
        margin-bottom: var(--space-1);
    }

    .hover-card__row {
        display: flex;
        justify-content: space-between;
        gap: var(--space-3);
        padding: 1px 0;
    }

    .hover-card__key {
        font-family: var(--font-mono);
        font-size: var(--text-2xs);
        color: var(--color-text-muted);
    }

    .hover-card__val {
        font-family: var(--font-mono);
        font-size: var(--text-2xs);
        color: var(--color-text);
    }
</style>
