<script lang="ts">
    let {
        geom,
        running,
        gen,
        step,
        stepsPerGen,
        pop,
    }: {
        geom: {
            x: number;
            y: number;
            w: number;
            h: number;
            cx: number;
            cy: number;
        };
        running: boolean;
        gen: number;
        step: number;
        stepsPerGen: number;
        pop: number;
    } = $props();

    function pad3(n: number): string {
        return n.toString().padStart(3, "0");
    }

    function fmtPop(n: number): string {
        return n.toLocaleString("fr-FR");
    }
</script>

<aside
    class="telemetry"
    style="top: {geom.y}px; left: {geom.x + geom.w + 12}px"
    aria-label="Simulation telemetry"
>
    <p class="telemetry__header small-caps">Telemetry</p>

    <dl class="telemetry__stats">
        <dt class="telemetry__key">gen</dt>
        <dd class="telemetry__val">{pad3(gen)}</dd>

        <dt class="telemetry__key">step</dt>
        <dd class="telemetry__val">
            {pad3(step)}<span class="telemetry__total">/{stepsPerGen}</span>
        </dd>

        <dt class="telemetry__key">pop</dt>
        <dd class="telemetry__val">{fmtPop(pop)}</dd>

        <dt class="telemetry__key">fps</dt>
        <dd class="telemetry__val" class:telemetry__val--accent={running}>
            {running ? "60" : "--"}
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

    .telemetry__key {
        font-family: var(--font-mono);
        font-size: 0.75rem;
        color: var(--color-text-muted);
    }

    .telemetry__val {
        font-family: var(--font-mono);
        font-size: 0.75rem;
        color: var(--color-text);
    }

    .telemetry__val--accent {
        color: var(--color-accent-text);
    }

    .telemetry__total {
        color: var(--color-text-muted);
    }
</style>
