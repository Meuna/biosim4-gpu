<script lang="ts">
    const SVG_H = 36;
    const PAD_Y = 2;
    const DOT_SPACING = 7;

    let {
        gen,
        survivalHistory,
        min,
        current,
        max,
        left,
        maxWidth,
    }: {
        gen: number;
        survivalHistory: number[];
        min: number | null;
        current: number | null;
        max: number | null;
        left: number;
        maxWidth: number;
    } = $props();

    function pad3(n: number): string {
        return n.toString().padStart(3, "0");
    }

    // Sparkline width grows one DOT_SPACING per generation, then clamps to the
    // grid-shared max width — so the dots fan out to the right while there is
    // room and compress once the container is full.
    const svgW = $derived(
        Math.min(
            Math.max(0, (survivalHistory.length - 1) * DOT_SPACING),
            maxWidth,
        ),
    );

    const y = (v: number): number => (1 - v) * (SVG_H - PAD_Y * 2) + PAD_Y;
    const x = (i: number): number =>
        survivalHistory.length < 2
            ? 0
            : (i / (survivalHistory.length - 1)) * svgW;

    /** Polyline points across the full survival history. */
    const sparklinePoints = $derived.by(() => {
        if (survivalHistory.length < 2) return "";
        return survivalHistory
            .map((v, i) => `${x(i).toFixed(1)},${y(v).toFixed(1)}`)
            .join(" ");
    });

    /** Intermediate data points (all but the last). */
    const sparklineDots = $derived.by(() => {
        if (survivalHistory.length < 2) return [] as { x: number; y: number }[];
        return survivalHistory
            .slice(0, -1)
            .map((v, i) => ({ x: x(i), y: y(v) }));
    });

    /** Last data point — pinned to the right edge, rendered in accent color. */
    const sparklineLast = $derived.by(() => {
        if (survivalHistory.length < 1) return null;
        return { x: svgW, y: y(survivalHistory[survivalHistory.length - 1]) };
    });

    const pct = (v: number | null): string =>
        v === null ? "—" : `${Math.round(v * 100)}%`;
</script>

<aside
    class="hud"
    style="left: {left}px; max-width: {maxWidth}px"
    aria-label="Survival history"
>
    <p class="hud__readout small-caps">
        gen <span class="hud__stat hud__stat--now">{pad3(gen)}</span>
    </p>
    <p class="hud__readout small-caps">
        survival
        <span class="hud__stat">min {pct(min)}</span>
        <span class="hud__stat hud__stat--now">now {pct(current)}</span>
        <span class="hud__stat">max {pct(max)}</span>
    </p>
    {#if sparklinePoints}
        <svg
            class="hud__sparkline"
            width={svgW}
            height={SVG_H}
            viewBox="0 0 {svgW} {SVG_H}"
            aria-label="Survival rate sparkline"
        >
            <polyline
                points={sparklinePoints}
                fill="none"
                stroke="var(--color-text)"
                stroke-width="1.2"
                stroke-opacity="0.6"
            />
            {#each sparklineDots as dot}
                <circle
                    cx={dot.x}
                    cy={dot.y}
                    r="1.2"
                    fill="var(--color-text)"
                />
            {/each}
            {#if sparklineLast}
                <circle
                    cx={sparklineLast.x}
                    cy={sparklineLast.y}
                    r="2.5"
                    fill="var(--color-accent)"
                />
            {/if}
        </svg>
    {:else}
        <div class="hud__sparkline-empty" aria-hidden="true">—</div>
    {/if}
</aside>

<style>
    .hud {
        position: fixed;
        bottom: var(--space-5);
        z-index: 15;
    }

    .hud__readout {
        margin: 0 0 var(--space-1) 0;
        display: flex;
        flex-wrap: wrap;
        gap: var(--space-3);
        /* .small-caps handles font/size/casing */
    }

    .hud__stat {
        color: var(--color-text-muted);
    }

    .hud__stat--now {
        color: var(--color-accent);
    }

    .hud__sparkline {
        display: block;
    }

    .hud__sparkline-empty {
        font-family: var(--font-mono);
        font-size: var(--text-xs);
        color: var(--color-text-muted);
    }
</style>
