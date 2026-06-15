<script lang="ts">
    const SVG_W = 180;
    const SVG_H = 36;
    const PAD_Y = 2;

    let {
        survivalHistory,
    }: {
        survivalHistory: number[];
    } = $props();

    /** Polyline points for the survival sparkline, normalized 0–1 → SVG coords */
    const sparklinePoints = $derived.by(() => {
        const h = survivalHistory;
        if (h.length < 2) return "";
        return h
            .map((v, i) => {
                const x = (i / (h.length - 1)) * SVG_W;
                const y = (1 - v) * (SVG_H - PAD_Y * 2) + PAD_Y;
                return `${x.toFixed(1)},${y.toFixed(1)}`;
            })
            .join(" ");
    });

    /** Intermediate data points (all but the last) */
    const sparklineDots = $derived.by(() => {
        const h = survivalHistory;
        if (h.length < 2) return [] as { x: number; y: number }[];
        return h.slice(0, -1).map((v, i) => ({
            x: (i / (h.length - 1)) * SVG_W,
            y: (1 - v) * (SVG_H - PAD_Y * 2) + PAD_Y,
        }));
    });

    /** Last data point — rendered in accent color */
    const sparklineLast = $derived.by(() => {
        const h = survivalHistory;
        if (h.length < 1) return null;
        const i = h.length - 1;
        return {
            x: SVG_W,
            y: (1 - h[i]) * (SVG_H - PAD_Y * 2) + PAD_Y,
        };
    });
</script>

<aside class="hud" aria-label="Survival history">
    <p class="hud__sparkline-label small-caps">
        survival · last {survivalHistory.length} gens
    </p>
    {#if sparklinePoints}
        <svg
            class="hud__sparkline"
            width={SVG_W}
            height={SVG_H}
            viewBox="0 0 {SVG_W} {SVG_H}"
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
        left: var(--space-6);
        bottom: var(--space-5);
        z-index: 15;
    }

    .hud__sparkline-label {
        margin: 0 0 var(--space-1) 0;
        /* .small-caps handles font/size/casing */
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
