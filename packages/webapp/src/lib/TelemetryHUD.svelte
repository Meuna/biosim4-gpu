<script lang="ts">
    import type { SimPhase } from "./simMachine.svelte";

    let {
        geom,
        placement = "right",
        phase,
        step,
        stepsPerGen,
        kills,
        fps = null,
    }: {
        geom: {
            x: number;
            y: number;
            w: number;
            h: number;
            cx: number;
            cy: number;
        };
        placement?: "right" | "below";
        phase: SimPhase;
        step: number;
        stepsPerGen: number;
        kills: number;
        fps?: number | null;
    } = $props();

    const running = $derived(phase === "STEPS_RUNNING");

    function pad3(n: number): string {
        return n.toString().padStart(3, "0");
    }

    function fmtKills(n: number): string {
        return n.toLocaleString("fr-FR");
    }
</script>

<aside
    class="telemetry"
    class:telemetry--below={placement === "below"}
    style={placement === "below"
        ? `top: ${geom.y + geom.h + 12}px; left: ${geom.x}px`
        : `top: ${geom.y}px; left: ${geom.x + geom.w + 12}px`}
    aria-label="Simulation telemetry"
>
    <p class="telemetry__header small-caps">Telemetry</p>

    <dl class="telemetry__stats">
        <dt class="telemetry__key">step</dt>
        <dd class="telemetry__val">
            {pad3(step)}<span class="telemetry__total">/{stepsPerGen}</span>
        </dd>

        <dt class="telemetry__key">kills</dt>
        <dd class="telemetry__val">{fmtKills(kills)}</dd>

        <dt class="telemetry__key">fps</dt>
        <dd class="telemetry__val" class:telemetry__val--accent={running}>
            {fps !== null ? Math.round(fps).toString() : "--"}
        </dd>
    </dl>
</aside>

<style>
    .telemetry {
        position: fixed;
        z-index: 15;
    }

    .telemetry__header {
        margin: 0 0 var(--space-2) 0;
        /* .small-caps handles font/size/casing */
    }

    .telemetry__stats {
        display: grid;
        grid-template-columns: auto auto;
        gap: var(--space-1) var(--space-6);
        margin: 0;
    }

    /* Below-grid: lay the stats out as a single wrapping horizontal row. */
    .telemetry--below .telemetry__stats {
        display: flex;
        flex-wrap: wrap;
        align-items: baseline;
        column-gap: var(--space-2);
        row-gap: var(--space-1);
    }

    /* Group each key→value pair: extra space before every key but the first. */
    .telemetry--below .telemetry__key:not(:first-of-type) {
        margin-left: var(--space-4);
    }

    .telemetry__key {
        font-family: var(--font-mono);
        font-size: var(--text-xs);
        color: var(--color-text-muted);
    }

    .telemetry__val {
        font-family: var(--font-mono);
        font-size: var(--text-xs);
        color: var(--color-text);
    }

    .telemetry__val--accent {
        color: var(--color-accent-text);
    }

    .telemetry__total {
        color: var(--color-text-muted);
    }
</style>
