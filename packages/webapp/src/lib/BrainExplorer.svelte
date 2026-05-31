<script lang="ts">
    // BrainExplorer — force-directed neural-network graph. Senses are pinned on
    // the left, actions on the right, internal neurons relax as a d3-force cloud
    // in between. Connections are directed (arrowheads) and signed (colour);
    // clicking a node highlights its incident connections.
    //
    // We stop d3's internal timer and drive ticks from our own requestAnimation-
    // Frame loop, which self-halts once alpha decays below alphaMin. Slider
    // changes nudge one force param and re-warm alpha — bounded per-frame work
    // (one tick) keeps the largest topology (20×128×16) smooth. The loop is
    // cancelled and the sim stopped on teardown, so no tick fires after unmount.

    import {
        forceSimulation,
        forceManyBody,
        forceLink,
        forceX,
        forceY,
        forceCollide,
        type Simulation,
        type SimulationNodeDatum,
        type SimulationLinkDatum,
    } from "d3-force";
    import { Skull } from "lucide-svelte";
    import ParamSlider from "./ParamSlider.svelte";
    import {
        GENE_IO,
        SENSOR_LABELS,
        ACTION_LABELS,
        ACTION_ICONS,
        brainSynthesis,
        type BrainConn,
        type ActionIconName,
    } from "./brain";

    let {
        conns,
        neuronCount,
        variant,
    }: {
        conns: BrainConn[];
        neuronCount: number;
        variant: "preview" | "full";
    } = $props();

    // ── Layout coordinate space (SVG viewBox units) ──────────────────────────
    const VW = 600;
    const VH = 420;
    const PAD = 34;
    const COL_L = 70;
    const COL_R = VW - 70;

    // ── Force knobs (full variant) + focus state ─────────────────────────────
    let chargeMag = $state(120);
    let linkDist = $state(60);
    let focusedId = $state<string | null>(null);

    interface GNode extends SimulationNodeDatum {
        id: string;
        kind: "sense" | "action" | "neuron";
        num: number;
        label: string;
        icon: ActionIconName | null;
        selfWeight: number | null;
    }
    interface GLink extends SimulationLinkDatum<GNode> {
        weight: number;
    }

    const ICON_COMPONENTS = { skull: Skull };

    // Markers are namespaced per variant so a mounted preview + full overlay
    // never collide on element ids (avoids module-level mutable state).
    const arrowPos = $derived(`be-arrow-pos-${variant}`);
    const arrowNeg = $derived(`be-arrow-neg-${variant}`);

    // d3 working arrays — plain (non-reactive): d3 owns and mutates them, and
    // startSim reads them inside the build $effect. Keeping them out of the
    // reactive graph avoids a self-triggering effect loop.
    let nodes: GNode[] = [];
    let links: GLink[] = [];
    let sim: Simulation<GNode, GLink> | null = null;

    // Reactive render snapshot (reassigned once per rebuild) — the template
    // reads node/link metadata from here, and coordinates from `pos`.
    let render = $state<{ nodes: GNode[]; links: GLink[] }>({
        nodes: [],
        links: [],
    });

    // Reactive position snapshot, reassigned each tick to drive re-render.
    let pos = $state<{ x: number; y: number }[]>([]);

    const synthesis = $derived(brainSynthesis(conns, neuronCount));

    function spread(count: number, i: number): number {
        return count <= 1 ? VH / 2 : PAD + ((VH - 2 * PAD) * i) / (count - 1);
    }

    function build(): void {
        const senseNums = new Set<number>();
        const actionNums = new Set<number>();
        for (const c of conns) {
            if (c.srcType === GENE_IO) senseNums.add(c.srcNum);
            if (c.sinkType === GENE_IO) actionNums.add(c.sinkNum);
        }
        const senseArr = [...senseNums].sort((a, b) => a - b);
        const actionArr = [...actionNums].sort((a, b) => a - b);

        const ns: GNode[] = [];
        const ids = new Set<string>();
        const add = (n: GNode): void => {
            ids.add(n.id);
            ns.push(n);
        };
        senseArr.forEach((num, i) =>
            add({
                id: `s${num}`,
                kind: "sense",
                num,
                label: SENSOR_LABELS[num] ?? `s${num}`,
                icon: null,
                selfWeight: null,
                fx: COL_L,
                fy: spread(senseArr.length, i),
            }),
        );
        actionArr.forEach((num, i) =>
            add({
                id: `a${num}`,
                kind: "action",
                num,
                label: ACTION_LABELS[num] ?? `a${num}`,
                icon: ACTION_ICONS[num] ?? null,
                selfWeight: null,
                fx: COL_R,
                fy: spread(actionArr.length, i),
            }),
        );
        for (let i = 0; i < neuronCount; i++) {
            add({
                id: `n${i}`,
                kind: "neuron",
                num: i,
                label: String(i),
                icon: null,
                selfWeight: null,
            });
        }

        const byId = new Map(ns.map((n) => [n.id, n]));
        const ls: GLink[] = [];
        for (const c of conns) {
            const sId = c.srcType === GENE_IO ? `s${c.srcNum}` : `n${c.srcNum}`;
            const tId =
                c.sinkType === GENE_IO ? `a${c.sinkNum}` : `n${c.sinkNum}`;
            if (!ids.has(sId) || !ids.has(tId)) continue;
            if (sId === tId) {
                // Self-loop (recurrent neuron→itself) — drawn as an arc badge,
                // not a d3 link (degenerate zero-length edge).
                byId.get(sId)!.selfWeight = c.weight;
                continue;
            }
            ls.push({ source: sId, target: tId, weight: c.weight });
        }
        nodes = ns;
        links = ls;
        render = { nodes: ns, links: ls };
    }

    // We drive ticks via our own rAF loop (d3's internal timer is stopped) so
    // teardown is deterministic — cancelling the frame guarantees no tick fires
    // into an unmounted component. The loop self-halts once alpha decays.
    let rafId: number | null = null;
    const canAnimate = typeof requestAnimationFrame === "function";

    function loop(): void {
        if (!sim) {
            rafId = null;
            return;
        }
        sim.tick();
        snapshot();
        rafId =
            sim.alpha() > sim.alphaMin() && canAnimate
                ? requestAnimationFrame(loop)
                : null;
    }

    function kick(): void {
        if (canAnimate && rafId === null) rafId = requestAnimationFrame(loop);
    }

    function stopLoop(): void {
        if (rafId !== null && canAnimate) cancelAnimationFrame(rafId);
        rafId = null;
        sim?.stop();
    }

    function startSim(): void {
        sim?.stop();
        sim = forceSimulation(nodes)
            .force("charge", forceManyBody().strength(-chargeMag))
            .force(
                "link",
                forceLink<GNode, GLink>(links)
                    .id((d) => d.id)
                    .distance(linkDist)
                    .strength(0.4),
            )
            .force("x", forceX(VW / 2).strength(0.05))
            .force("y", forceY(VH / 2).strength(0.07))
            .force("collide", forceCollide(12))
            .stop();
        // forceSimulation() assigns initial (deterministic phyllotaxis)
        // positions in its constructor, so snapshot immediately — the first
        // paint and tests see positioned nodes without waiting for the loop.
        snapshot();
        kick();
    }

    function snapshot(): void {
        pos = nodes.map((n) => ({ x: n.x ?? VW / 2, y: n.y ?? VH / 2 }));
    }

    // Rebuild + restart whenever the agent's brain changes.
    $effect(() => {
        build();
        startSim();
        return stopLoop;
    });

    // Reheat on knob changes (single force-param update + low-alpha restart).
    $effect(() => {
        const cm = chargeMag;
        const ld = linkDist;
        if (!sim) return;
        (sim.force("charge") as ReturnType<typeof forceManyBody>).strength(-cm);
        (
            sim.force("link") as ReturnType<typeof forceLink<GNode, GLink>>
        ).distance(ld);
        sim.alpha(0.3);
        kick();
    });

    // Neighbour set of the focused node (itself + directly connected nodes).
    const neighbors = $derived.by(() => {
        if (!focusedId) return null;
        const s = new Set<string>([focusedId]);
        for (const l of render.links) {
            const sid = (l.source as GNode).id;
            const tid = (l.target as GNode).id;
            if (sid === focusedId) s.add(tid);
            if (tid === focusedId) s.add(sid);
        }
        return s;
    });

    function linkDimmed(l: GLink): boolean {
        if (!focusedId) return false;
        return (
            (l.source as GNode).id !== focusedId &&
            (l.target as GNode).id !== focusedId
        );
    }

    function nodeDimmed(n: GNode): boolean {
        return neighbors !== null && !neighbors.has(n.id);
    }

    function strokeWidth(weight: number): number {
        return 0.6 + Math.min(Math.abs(weight), 4) * 0.6;
    }

    function toggleFocus(id: string): void {
        focusedId = focusedId === id ? null : id;
    }
</script>

<div class="brain" class:brain--full={variant === "full"}>
    {#if variant === "full"}
        <div class="brain__controls">
            <ParamSlider
                label="charge"
                hint="node repulsion"
                min={20}
                max={400}
                step={10}
                value={chargeMag}
                format={(v) => String(Math.round(v))}
                onchange={(v) => (chargeMag = v)}
            />
            <ParamSlider
                label="link dist"
                hint="edge length"
                min={20}
                max={200}
                step={5}
                value={linkDist}
                format={(v) => String(Math.round(v))}
                onchange={(v) => (linkDist = v)}
            />
            <button
                type="button"
                class="button button--ghost brain__reheat"
                onclick={() => {
                    sim?.alpha(0.6);
                    kick();
                }}
            >
                reheat
            </button>
        </div>
    {/if}

    <p class="brain__synthesis small-caps">
        {synthesis.senses} senses → {synthesis.internal} internal → {synthesis.actions}
        actions · {synthesis.connections} connections
    </p>

    <svg
        class="brain__svg"
        viewBox="0 0 {VW} {VH}"
        preserveAspectRatio="xMidYMid meet"
        role="img"
        aria-label="Agent brain network: {synthesis.senses} senses, {synthesis.internal} internal neurons, {synthesis.actions} actions, {synthesis.connections} connections"
    >
        <defs>
            <marker
                id={arrowPos}
                viewBox="0 0 10 10"
                refX="9"
                refY="5"
                markerWidth="6"
                markerHeight="6"
                orient="auto-start-reverse"
            >
                <path d="M0,0 L10,5 L0,10 z" fill="var(--color-accent)" />
            </marker>
            <marker
                id={arrowNeg}
                viewBox="0 0 10 10"
                refX="9"
                refY="5"
                markerWidth="6"
                markerHeight="6"
                orient="auto-start-reverse"
            >
                <path d="M0,0 L10,5 L0,10 z" fill="var(--color-warn)" />
            </marker>
        </defs>

        <!-- Background: click (or Esc) to clear focus -->
        <rect
            x="0"
            y="0"
            width={VW}
            height={VH}
            fill="transparent"
            role="button"
            tabindex="-1"
            aria-label="Clear focus"
            onclick={() => (focusedId = null)}
            onkeydown={(e) => {
                if (e.key === "Escape") focusedId = null;
            }}
        />

        <!-- Connections -->
        <g class="brain__links">
            {#each render.links as link, i (i)}
                {@const si = (link.source as GNode).index ?? 0}
                {@const ti = (link.target as GNode).index ?? 0}
                {#if pos[si] && pos[ti]}
                    <line
                        x1={pos[si].x}
                        y1={pos[si].y}
                        x2={pos[ti].x}
                        y2={pos[ti].y}
                        stroke={link.weight >= 0
                            ? "var(--color-accent)"
                            : "var(--color-warn)"}
                        stroke-width={strokeWidth(link.weight)}
                        class:brain__edge--dim={linkDimmed(link)}
                        marker-end="url(#{link.weight >= 0
                            ? arrowPos
                            : arrowNeg})"
                    />
                {/if}
            {/each}
        </g>

        <!-- Nodes -->
        <g class="brain__nodes">
            {#each render.nodes as n, i (n.id)}
                {#if pos[i]}
                    <g
                        class="brain__node"
                        class:brain__node--dim={nodeDimmed(n)}
                        class:brain__node--focus={focusedId === n.id}
                        transform="translate({pos[i].x}, {pos[i].y})"
                        onclick={() => toggleFocus(n.id)}
                        onkeydown={(e) => {
                            if (e.key === "Enter" || e.key === " ") {
                                e.preventDefault();
                                toggleFocus(n.id);
                            }
                        }}
                        role="button"
                        tabindex="0"
                        aria-label="{n.kind} {n.label}"
                    >
                        {#if n.selfWeight !== null}
                            <circle
                                class="brain__selfloop"
                                cx="0"
                                cy="-13"
                                r="6"
                                fill="none"
                                stroke={n.selfWeight >= 0
                                    ? "var(--color-accent)"
                                    : "var(--color-warn)"}
                                stroke-width="1.2"
                            />
                        {/if}
                        {#if n.kind === "neuron"}
                            <circle
                                r="5"
                                fill="var(--color-surface)"
                                stroke="var(--color-text)"
                                stroke-width="1"
                            />
                        {:else if n.kind === "sense"}
                            <circle
                                r="9"
                                fill="var(--color-surface)"
                                stroke="var(--color-text)"
                                stroke-width="1.4"
                            />
                        {:else}
                            <circle r="9" fill="var(--color-text)" />
                        {/if}

                        {#if n.icon}
                            {@const Icon = ICON_COMPONENTS[n.icon]}
                            <foreignObject x="-7" y="-7" width="14" height="14">
                                <Icon size={14} color="var(--color-surface)" />
                            </foreignObject>
                        {:else if n.kind !== "neuron"}
                            <text
                                class="brain__label"
                                class:brain__label--invert={n.kind === "action"}
                                text-anchor="middle"
                                dominant-baseline="central"
                            >
                                {n.label}
                            </text>
                        {/if}
                    </g>
                {/if}
            {/each}
        </g>
    </svg>

    {#if variant === "full"}
        <div class="brain__legend small-caps">
            <span class="brain__legend-item">
                <svg width="22" height="8" aria-hidden="true">
                    <line
                        x1="0"
                        y1="4"
                        x2="22"
                        y2="4"
                        stroke="var(--color-accent)"
                        stroke-width="2"
                    /></svg
                >
                excitatory
            </span>
            <span class="brain__legend-item">
                <svg width="22" height="8" aria-hidden="true">
                    <line
                        x1="0"
                        y1="4"
                        x2="22"
                        y2="4"
                        stroke="var(--color-warn)"
                        stroke-width="2"
                    /></svg
                >
                inhibitory
            </span>
            <span class="brain__legend-item">→ direction</span>
            <span class="brain__legend-item">thickness ∝ |weight|</span>
        </div>
    {/if}
</div>

<style>
    .brain {
        display: flex;
        flex-direction: column;
        width: 100%;
    }

    .brain--full {
        height: 100%;
    }

    .brain__controls {
        display: flex;
        align-items: flex-end;
        gap: var(--space-6);
        flex-wrap: wrap;
        margin-bottom: var(--space-3);
    }

    .brain__controls :global(.field-row) {
        flex: 1;
        min-width: 12rem;
    }

    .brain__reheat {
        flex-shrink: 0;
    }

    .brain__synthesis {
        margin: 0 0 var(--space-2) 0;
        color: var(--color-text-muted);
    }

    .brain__svg {
        width: 100%;
        flex: 1;
        min-height: 0;
        border: 1px solid var(--color-border-subtle);
        border-radius: var(--radius-md);
        background: var(--color-surface);
    }

    .brain:not(.brain--full) .brain__svg {
        height: 220px;
    }

    .brain__edge--dim {
        opacity: 0.08;
    }

    .brain__node {
        cursor: pointer;
    }

    .brain__node--dim {
        opacity: 0.18;
    }

    .brain__node--focus circle {
        stroke: var(--color-accent);
        stroke-width: 2;
    }

    .brain__label {
        font-family: var(--font-mono);
        font-size: 7px;
        fill: var(--color-text);
        pointer-events: none;
    }

    .brain__label--invert {
        fill: var(--color-surface);
    }

    .brain__legend {
        display: flex;
        flex-wrap: wrap;
        gap: var(--space-4);
        margin-top: var(--space-3);
        color: var(--color-text-muted);
    }

    .brain__legend-item {
        display: inline-flex;
        align-items: center;
        gap: var(--space-2);
    }
</style>
