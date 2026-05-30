<script lang="ts">
    import type { BrainModel } from "./brainModel";
    import { brainCounts } from "./brainModel";
    import { layoutBrain } from "./brainLayout";

    let {
        model,
        onExpand,
    }: {
        model: BrainModel;
        onExpand: () => void;
    } = $props();

    // Diagram mode is reserved for small, legible brains; everything larger
    // renders as an abstract fingerprint. Thresholds per the gh-72 spec.
    const DIAGRAM_MAX_NODES = 16;
    const DIAGRAM_MAX_CONNS = 30;

    // SVG drawing box and inner margins (layout space [0,1] maps into these).
    const VB_W = 320;
    const VB_H = 200;
    const PAD_X = 26;
    const PAD_Y = 16;

    const counts = $derived(brainCounts(model));
    const isDiagram = $derived(
        counts.nodes <= DIAGRAM_MAX_NODES &&
            counts.connections <= DIAGRAM_MAX_CONNS,
    );

    const mapX = (x: number) => PAD_X + x * (VB_W - 2 * PAD_X);
    const mapY = (y: number) => PAD_Y + y * (VB_H - 2 * PAD_Y);
    // |weight| (≈0..4) → stroke width in px.
    const strokeFor = (w: number) => 0.4 + Math.min(Math.abs(w), 4) * (1.4 / 4);

    interface DiagramNode {
        id: number;
        cx: number;
        cy: number;
        type: "sense" | "internal" | "action";
    }
    interface DiagramEdge {
        d: string;
        width: number;
        dashed: boolean;
    }

    const diagram = $derived.by(() => {
        const layout = layoutBrain(model);
        const nodes: DiagramNode[] = model.neurons.map((n) => {
            const p = layout.positions.get(n.id)!;
            return { id: n.id, cx: mapX(p.x), cy: mapY(p.y), type: n.type };
        });
        const edges: DiagramEdge[] = layout.edges.map((e) => {
            const a = layout.positions.get(e.source)!;
            const b = layout.positions.get(e.sink)!;
            const ax = mapX(a.x);
            const ay = mapY(a.y);
            const bx = mapX(b.x);
            const by = mapY(b.y);
            if (e.routing === "self") {
                // Small arc bulging to the right of the node.
                const r = 8;
                const d = `M ${ax} ${ay - 4} A ${r} ${r} 0 1 1 ${ax} ${ay + 4}`;
                return { d, width: strokeFor(e.weight), dashed: e.weight < 0 };
            }
            // Quadratic curve, control point offset perpendicular by the bow.
            const mx = (ax + bx) / 2;
            const my = (ay + by) / 2;
            const dx = bx - ax;
            const dy = by - ay;
            const len = Math.hypot(dx, dy) || 1;
            const off = e.bow * (VB_W - 2 * PAD_X);
            const cxp = mx + (-dy / len) * off;
            const cyp = my + (dx / len) * off;
            return {
                d: `M ${ax} ${ay} Q ${cxp} ${cyp} ${bx} ${by}`,
                width: strokeFor(e.weight),
                dashed: e.weight < 0,
            };
        });
        return { nodes, edges };
    });

    // ── Fingerprint (signature) mode ──
    // Three vertical stacks of marks, sized to counts, with a sampled, bundled
    // set of faint strokes conveying wiring density (NOT one line per edge).
    const STACK_X = { sense: 60, internal: 160, action: 260 } as const;
    const FP_TOP = 22;
    const FP_BOT = 178;
    const MAX_BUNDLE = 28;

    function slotY(index: number, total: number): number {
        if (total <= 1) return (FP_TOP + FP_BOT) / 2;
        return FP_TOP + (index / (total - 1)) * (FP_BOT - FP_TOP);
    }

    const fingerprint = $derived.by(() => {
        // Per-type ordinal index for each neuron id.
        const order = new Map<number, { x: number; y: number }>();
        const seen = { sense: 0, internal: 0, action: 0 };
        const totals = {
            sense: counts.sense,
            internal: counts.internal,
            action: counts.action,
        };
        for (const n of model.neurons) {
            const i = seen[n.type]++;
            order.set(n.id, {
                x: STACK_X[n.type],
                y: slotY(i, totals[n.type]),
            });
        }
        const marks = model.neurons.map((n) => ({
            type: n.type,
            ...order.get(n.id)!,
        }));
        // Evenly sample connections to bundle into faint strokes.
        const conns = model.connections;
        const stride = Math.max(1, Math.ceil(conns.length / MAX_BUNDLE));
        const bundles: { d: string; dashed: boolean }[] = [];
        for (let i = 0; i < conns.length; i += stride) {
            const c = conns[i];
            const a = order.get(c.source);
            const b = order.get(c.sink);
            if (!a || !b) continue;
            const mx = (a.x + b.x) / 2;
            bundles.push({
                d: `M ${a.x} ${a.y} Q ${mx} ${(a.y + b.y) / 2} ${b.x} ${b.y}`,
                dashed: c.weight < 0,
            });
        }
        return { marks, bundles };
    });

    const headerLine = $derived(
        `BRAIN · ${counts.sense} IN → ${counts.internal} HIDDEN → ` +
            `${counts.action} OUT · ${counts.connections} CONN`,
    );
</script>

<div class="brain-sig">
    <p class="brain-sig__header">{headerLine}</p>

    <div class="brain-sig__frame">
        <svg
            width="100%"
            height="200"
            viewBox="0 0 {VB_W} {VB_H}"
            preserveAspectRatio="xMidYMid meet"
            class="brain-sig__svg"
            aria-label={isDiagram
                ? "Brain diagram"
                : "Brain signature fingerprint"}
            role="img"
        >
            {#if isDiagram}
                {#each diagram.edges as e (e.d)}
                    <path
                        d={e.d}
                        fill="none"
                        stroke="var(--color-canvas-edge)"
                        stroke-width={e.width}
                        stroke-dasharray={e.dashed ? "3 2" : "none"}
                    />
                {/each}
                {#each diagram.nodes as n (n.id)}
                    {#if n.type === "sense"}
                        <circle
                            cx={n.cx}
                            cy={n.cy}
                            r="4"
                            fill="none"
                            stroke="var(--color-text)"
                            stroke-width="1"
                        />
                    {:else if n.type === "action"}
                        <circle
                            cx={n.cx}
                            cy={n.cy}
                            r="4"
                            fill="var(--color-text)"
                        />
                    {:else}
                        <circle
                            cx={n.cx}
                            cy={n.cy}
                            r="3"
                            fill="var(--color-surface)"
                            stroke="var(--color-text)"
                            stroke-width="1"
                        />
                    {/if}
                {/each}
            {:else}
                {#each fingerprint.bundles as b, i (i)}
                    <path
                        d={b.d}
                        fill="none"
                        stroke="var(--color-canvas-edge)"
                        stroke-width="0.6"
                        stroke-opacity="0.35"
                        stroke-dasharray={b.dashed ? "2 2" : "none"}
                    />
                {/each}
                {#each fingerprint.marks as m, i (i)}
                    <line
                        x1={m.x - 5}
                        y1={m.y}
                        x2={m.x + 5}
                        y2={m.y}
                        stroke="var(--color-text)"
                        stroke-width={m.type === "action" ? 2 : 1}
                        stroke-opacity={m.type === "internal" ? 0.6 : 1}
                    />
                {/each}
            {/if}
        </svg>

        <button
            class="brain-sig__expand"
            onclick={onExpand}
            aria-label="Expand brain to full-screen explorer"
        >
            ⊞ expand
        </button>
    </div>
</div>

<style>
    .brain-sig {
        width: 100%;
    }

    .brain-sig__header {
        font-family: var(--font-mono);
        font-size: 0.625rem;
        letter-spacing: 0.08em;
        color: var(--color-text-muted);
        margin: 0 0 var(--space-2) 0;
        white-space: nowrap;
        overflow: hidden;
        text-overflow: ellipsis;
    }

    .brain-sig__frame {
        position: relative;
        width: 100%;
        height: 200px;
        border: 1px solid var(--color-border-subtle);
        border-radius: var(--radius-md);
        overflow: hidden;
        background:
            linear-gradient(var(--color-border-subtle) 1px, transparent 1px) 0
                0 / 12px 12px,
            linear-gradient(
                    90deg,
                    var(--color-border-subtle) 1px,
                    transparent 1px
                )
                0 0 / 12px 12px;
    }

    .brain-sig__svg {
        position: absolute;
        inset: 0;
    }

    .brain-sig__expand {
        position: absolute;
        right: var(--space-2);
        bottom: var(--space-2);
        border: 0;
        background: transparent;
        cursor: pointer;
        font-family: var(--font-mono);
        font-size: 9px;
        color: var(--color-text-muted);
        padding: var(--space-1);
        transition: color 0.1s;
    }

    .brain-sig__expand:hover {
        color: var(--color-accent-text);
    }
</style>
