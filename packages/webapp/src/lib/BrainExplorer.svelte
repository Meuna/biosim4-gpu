<script lang="ts">
    // BrainExplorer — force-directed neural-network graph. Senses are pinned on
    // the left, actions on the right, internal neurons relax as a d3-force cloud
    // in between. Connections are directed (arrowheads) and signed (blue =
    // excitatory, red = inhibitory); clicking a node highlights its incident
    // connections and shows its full sensor/action name as muted text.
    //
    // We stop d3's internal timer and drive ticks from our own requestAnimation-
    // Frame loop, which self-halts once alpha decays below alphaMin. Slider
    // changes nudge one force param and re-warm alpha — bounded per-frame work
    // (one tick) keeps the largest topology (20×128×16) smooth. The loop is
    // cancelled and the sim stopped on teardown, so no tick fires after unmount.

    import { untrack } from "svelte";
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
    import {
        ArrowBigDownDash,
        ArrowBigUpDash,
        CornerUpLeft,
        CornerUpRight,
        Dices,
        Dna,
        Expand,
        HouseWifi,
        LoaderPinwheel,
        Maximize2,
        MoveHorizontal,
        MoveVertical,
        MoveDown,
        MoveLeft,
        MoveRight,
        MoveUp,
        Radar,
        Radio,
        RadioTower,
        RefreshCcw,
        Skull,
        UnfoldHorizontal,
        UnfoldVertical,
        Wifi,
    } from "lucide-svelte";
    import {
        GENE_IO,
        SENSOR_LABELS,
        ACTION_LABELS,
        SENSOR_NAMES,
        ACTION_NAMES,
        glyphName,
        brainSynthesis,
        type BrainConn,
    } from "./brain";

    let {
        conns,
        neuronCount,
        variant,
        onExpand,
    }: {
        conns: BrainConn[];
        neuronCount: number;
        variant: "preview" | "full";
        onExpand?: () => void;
    } = $props();

    // ── Layout coordinate space (SVG viewBox units) ──────────────────────────
    const VW = 1000;
    const VH = 640;
    const TOP = 70;
    const BOT = VH - 50;
    const COL_L = 110;
    const COL_R = VW - 110;
    const R_IO = 12;
    const R_INT = 7;
    const ARROW_GAP = 2; // so the arrow tip just touches the target circle
    const PREVIEW_GROW = 1.8; // selected sense/action enlarges in the small view

    // ── Force knobs (full variant) + focus state ─────────────────────────────
    let chargeMag = $state(180);
    let linkDist = $state(80);
    let focusedId = $state<string | null>(null);

    // Stage pixel size (for mapping viewBox coords → the floating name card).
    let stageW = $state(0);
    let stageH = $state(0);

    interface GNode extends SimulationNodeDatum {
        id: string;
        kind: "sense" | "action" | "neuron";
        num: number;
        label: string;
        selfWeight: number | null;
    }
    interface GLink extends SimulationLinkDatum<GNode> {
        weight: number;
    }

    const ICON_COMPONENTS = {
        "arrow-big-up-dash": ArrowBigUpDash,
        "arrow-big-down-dash": ArrowBigDownDash,
        "corner-up-right": CornerUpRight,
        "corner-up-left": CornerUpLeft,
        dices: Dices,
        dna: Dna,
        expand: Expand,
        "house-wifi": HouseWifi,
        "loader-pinwheel": LoaderPinwheel,
        "move-horizontal": MoveHorizontal,
        "move-vertical": MoveVertical,
        "move-down": MoveDown,
        "move-left": MoveLeft,
        "move-right": MoveRight,
        "move-up": MoveUp,
        radar: Radar,
        radio: Radio,
        "radio-tower": RadioTower,
        "refresh-ccw": RefreshCcw,
        skull: Skull,
        "unfold-horizontal": UnfoldHorizontal,
        "unfold-vertical": UnfoldVertical,
        wifi: Wifi,
    };

    // Marker is namespaced per variant so a mounted preview + full overlay never
    // collide on element ids (avoids module-level mutable state).
    const arrowId = $derived(`be-arrow-${variant}`);

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

    function radius(kind: GNode["kind"]): number {
        return kind === "neuron" ? R_INT : R_IO;
    }

    // Selected sense/action nodes grow in the small preview for readability.
    function nodeScale(n: GNode): number {
        return variant === "preview" &&
            focusedId === n.id &&
            n.kind !== "neuron"
            ? PREVIEW_GROW
            : 1;
    }

    // Self-loop drawn as a curved arrow above the node (local coords, node at
    // origin). Gets the shared arrowhead via marker-end.
    function selfLoopPath(r: number): string {
        const sx = -r * 0.5;
        const sy = -r * 0.87;
        const ex = r * 0.5;
        const ey = -r * 0.87;
        const l = r * 2.4;
        return `M${sx},${sy} C${-l},${-l * 2.4} ${l},${-l * 2.4} ${ex},${ey}`;
    }

    function spread(count: number, i: number): number {
        return count <= 1
            ? (TOP + BOT) / 2
            : TOP + ((BOT - TOP) * i) / (count - 1);
    }

    function fullName(n: GNode): string {
        if (n.kind === "neuron") return `Internal neuron #${n.num}`;
        if (n.kind === "sense") return SENSOR_NAMES[n.num] ?? n.label;
        return ACTION_NAMES[n.num] ?? n.label;
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
        const pushNode = (n: GNode): void => {
            ids.add(n.id);
            ns.push(n);
        };
        senseArr.forEach((num, i) =>
            pushNode({
                id: `s${num}`,
                kind: "sense",
                num,
                label: SENSOR_LABELS[num] ?? `s${num}`,
                selfWeight: null,
                fx: COL_L,
                fy: spread(senseArr.length, i),
            }),
        );
        actionArr.forEach((num, i) =>
            pushNode({
                id: `a${num}`,
                kind: "action",
                num,
                label: ACTION_LABELS[num] ?? `a${num}`,
                selfWeight: null,
                fx: COL_R,
                fy: spread(actionArr.length, i),
            }),
        );
        // Seed neurons on a deterministic golden-angle spiral filling the
        // central band — derived from the layout coords (NOT the viewBox origin),
        // so a rebuild never flings nodes in from the top-left corner.
        const cx = (COL_L + COL_R) / 2;
        const cy = (TOP + BOT) / 2;
        const rx = (COL_R - COL_L) / 5;
        const ry = (BOT - TOP) / 4;
        const goldenAngle = Math.PI * (3 - Math.sqrt(5));
        for (let i = 0; i < neuronCount; i++) {
            const a = i * goldenAngle;
            const rad = Math.sqrt((i + 0.5) / Math.max(neuronCount, 1));
            pushNode({
                id: `n${i}`,
                kind: "neuron",
                num: i,
                label: String(i),
                selfWeight: null,
                x: cx + Math.cos(a) * rx * rad,
                y: cy + Math.sin(a) * ry * rad,
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
        // Read the knobs untracked: this runs inside the build $effect, and
        // subscribing here would rebuild the whole graph on every slider move
        // (flinging nodes back to the origin). The reheat $effect owns updates.
        const charge = untrack(() => chargeMag);
        const dist = untrack(() => linkDist);
        sim = forceSimulation(nodes)
            .force("charge", forceManyBody().strength(-charge))
            .force(
                "link",
                forceLink<GNode, GLink>(links)
                    .id((d) => d.id)
                    .distance(dist)
                    .strength(0.18),
            )
            .force("x", forceX(VW / 2).strength(0.05))
            .force("y", forceY(VH / 2).strength(0.06))
            .force(
                "collide",
                forceCollide<GNode>((d) => radius(d.kind) + 3),
            )
            .stop();
        // forceSimulation() assigns initial positions in its constructor, so
        // snapshot immediately — the first paint and tests see positioned nodes
        // without waiting for the loop.
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

    // Reheat on knob changes (single force-param update + re-warm; no rebuild).
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

    const focusedIdx = $derived(
        focusedId ? render.nodes.findIndex((n) => n.id === focusedId) : -1,
    );
    const focusedNode = $derived(
        focusedIdx >= 0 ? render.nodes[focusedIdx] : null,
    );

    // Name shown for a selected sense/action (neurons show nothing).
    const nameText = $derived(
        focusedNode && focusedNode.kind !== "neuron"
            ? fullName(focusedNode)
            : null,
    );

    // Name position (stage pixels). Full view: just under the node (viewBox →
    // pixels, xMidYMid meet). Preview: pinned top-centre (the graph is too small
    // to label in place).
    const namePos = $derived.by(() => {
        if (!nameText) return null;
        if (variant === "preview") return { left: stageW / 2, top: 6 };
        if (focusedIdx < 0 || !pos[focusedIdx] || !stageW || !stageH) {
            return null;
        }
        const scale = Math.min(stageW / VW, stageH / VH);
        const offX = (stageW - VW * scale) / 2;
        const offY = (stageH - VH * scale) / 2;
        return {
            left: offX + pos[focusedIdx].x * scale,
            top: offY + pos[focusedIdx].y * scale + R_IO * scale + 6,
        };
    });

    function linkIncident(l: GLink): boolean {
        return (
            (l.source as GNode).id === focusedId ||
            (l.target as GNode).id === focusedId
        );
    }

    function linkOpacity(l: GLink): number {
        if (!focusedId) return 0.5;
        return linkIncident(l) ? 1 : 0.05;
    }

    function selfLinkOpacity(n: GNode): number {
        if (!focusedId) return 0.5;
        return focusedId === n.id ? 1 : 0.05;
    }

    function nodeDimmed(n: GNode): boolean {
        return neighbors !== null && !neighbors.has(n.id);
    }

    function strokeWidth(weight: number): number {
        return 0.75 + (Math.min(Math.abs(weight), 4) / 4) * 3.25;
    }

    interface EdgeGeom {
        x1: number;
        y1: number;
        x2: number;
        y2: number;
    }
    function edgeGeom(
        a: { x: number; y: number },
        b: { x: number; y: number },
        ra: number,
        rb: number,
    ): EdgeGeom {
        const dx = b.x - a.x;
        const dy = b.y - a.y;
        const len = Math.hypot(dx, dy) || 1;
        const ux = dx / len;
        const uy = dy / len;
        return {
            x1: a.x + ux * ra,
            y1: a.y + uy * ra,
            x2: b.x - ux * (rb + ARROW_GAP),
            y2: b.y - uy * (rb + ARROW_GAP),
        };
    }

    function toggleFocus(id: string): void {
        focusedId = focusedId === id ? null : id;
    }
</script>

<div class="brain" class:brain--full={variant === "full"}>
    {#if variant === "full"}
        <div class="brain__controls">
            <label class="brain__knob">
                charge
                <input
                    type="range"
                    min={20}
                    max={600}
                    step={10}
                    value={chargeMag}
                    aria-label="charge"
                    oninput={(e) => (chargeMag = +e.currentTarget.value)}
                />
            </label>
            <label class="brain__knob">
                link dist
                <input
                    type="range"
                    min={20}
                    max={160}
                    step={5}
                    value={linkDist}
                    aria-label="link dist"
                    oninput={(e) => (linkDist = +e.currentTarget.value)}
                />
            </label>
        </div>
    {/if}

    <p class="brain__synthesis small-caps">
        {synthesis.senses} senses → {synthesis.internal} internal → {synthesis.actions}
        actions · {synthesis.connections} connections
    </p>

    <div class="brain__stage">
        <svg
            class="brain__svg"
            bind:clientWidth={stageW}
            bind:clientHeight={stageH}
            viewBox="0 0 {VW} {VH}"
            preserveAspectRatio="xMidYMid meet"
            role="img"
            aria-label="Agent brain network: {synthesis.senses} senses, {synthesis.internal} internal neurons, {synthesis.actions} actions, {synthesis.connections} connections"
        >
            <defs>
                <marker
                    id={arrowId}
                    viewBox="0 0 10 10"
                    refX="9"
                    refY="5"
                    markerWidth="7"
                    markerHeight="7"
                    markerUnits="userSpaceOnUse"
                    orient="auto"
                >
                    <path d="M0,0 L10,5 L0,10 z" fill="context-stroke" />
                </marker>
            </defs>

            {#if variant === "full"}
                <text
                    class="brain__caption"
                    x={COL_L}
                    y={TOP - 28}
                    text-anchor="middle">SENSE</text
                >
                <text
                    class="brain__caption"
                    x={COL_R}
                    y={TOP - 28}
                    text-anchor="middle">ACTION</text
                >
            {/if}

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
                    {@const s = link.source as GNode}
                    {@const t = link.target as GNode}
                    {@const ps = pos[s.index ?? 0]}
                    {@const pt = pos[t.index ?? 0]}
                    {#if ps && pt}
                        {@const g = edgeGeom(
                            ps,
                            pt,
                            radius(s.kind),
                            radius(t.kind),
                        )}
                        <line
                            x1={g.x1}
                            y1={g.y1}
                            x2={g.x2}
                            y2={g.y2}
                            stroke={link.weight >= 0
                                ? "var(--color-link-pos)"
                                : "var(--color-link-neg)"}
                            stroke-width={strokeWidth(link.weight)}
                            opacity={linkOpacity(link)}
                            marker-end="url(#{arrowId})"
                        />
                    {/if}
                {/each}
            </g>

            <!-- Nodes -->
            <g class="brain__nodes">
                {#each render.nodes as n, i (n.id)}
                    {#if pos[i]}
                        {@const glyph = glyphName(n.label)}
                        <g
                            class="brain__node"
                            class:brain__node--dim={nodeDimmed(n)}
                            class:brain__node--focus={focusedId === n.id}
                            class:brain__node--neighbor={focusedId !== n.id &&
                                neighbors?.has(n.id)}
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
                                <path
                                    class="brain__selfloop"
                                    d={selfLoopPath(radius(n.kind))}
                                    fill="none"
                                    stroke={n.selfWeight >= 0
                                        ? "var(--color-link-pos)"
                                        : "var(--color-link-neg)"}
                                    stroke-width={strokeWidth(
                                        n.selfWeight ?? 0,
                                    )}
                                    opacity={selfLinkOpacity(n)}
                                    marker-end="url(#{arrowId})"
                                />
                            {/if}
                            <g transform="scale({nodeScale(n)})">
                                {#if n.kind === "neuron"}
                                    <circle
                                        r={R_INT}
                                        fill="var(--color-surface)"
                                        stroke="var(--color-text)"
                                        stroke-width="1"
                                    />
                                {:else if n.kind === "sense"}
                                    <circle
                                        r={R_IO}
                                        fill="var(--color-surface)"
                                        stroke="var(--color-text)"
                                        stroke-width="1.5"
                                    />
                                {:else}
                                    <circle r={R_IO} fill="var(--color-text)" />
                                {/if}

                                {#if glyph}
                                    {@const Icon =
                                        ICON_COMPONENTS[
                                            glyph as keyof typeof ICON_COMPONENTS
                                        ]}
                                    {#if Icon}
                                        <foreignObject
                                            x={-R_IO + 2}
                                            y={-R_IO + 2}
                                            width={R_IO * 2 - 4}
                                            height={R_IO * 2 - 4}
                                        >
                                            <Icon
                                                size={R_IO * 2 - 4}
                                                color={n.kind === "action"
                                                    ? "var(--color-surface)"
                                                    : "var(--color-text)"}
                                            />
                                        </foreignObject>
                                    {/if}
                                {:else if n.kind !== "neuron"}
                                    <text
                                        class="brain__label"
                                        class:brain__label--invert={n.kind ===
                                            "action"}
                                        text-anchor="middle"
                                        dominant-baseline="central"
                                    >
                                        {n.label}
                                    </text>
                                {/if}
                            </g>
                        </g>
                    {/if}
                {/each}
            </g>
        </svg>

        {#if nameText && namePos}
            <div
                class="brain__name"
                style="left: {namePos.left}px; top: {namePos.top}px"
            >
                {nameText}
            </div>
        {/if}

        {#if variant === "preview" && onExpand}
            <button
                type="button"
                class="brain__expand"
                onclick={onExpand}
                aria-label="Expand brain explorer"
                title="Expand brain explorer"
            >
                <Maximize2 size={9} />
            </button>
        {/if}
    </div>
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
        flex-wrap: wrap;
        gap: var(--space-6);
        margin-bottom: var(--space-3);
    }

    .brain__knob {
        display: inline-flex;
        align-items: center;
        gap: var(--space-2);
        font-family: var(--font-mono);
        font-size: 0.625rem;
        letter-spacing: 0.1em;
        text-transform: uppercase;
        color: var(--color-text-muted);
    }

    .brain__knob input[type="range"] {
        width: 120px;
        accent-color: var(--color-text);
    }

    .brain__knob input[type="range"]:active,
    .brain__knob input[type="range"]:focus-visible {
        accent-color: var(--color-accent);
    }

    .brain__synthesis {
        margin: 0 0 var(--space-2) 0;
        color: var(--color-text-muted);
    }

    .brain__stage {
        position: relative;
        width: 100%;
    }

    .brain--full .brain__stage {
        flex: 1;
        min-height: 0;
    }

    .brain__svg {
        display: block;
        width: 100%;
        border: 1px solid var(--color-border-subtle);
        border-radius: var(--radius-md);
        background: var(--color-surface);
    }

    .brain:not(.brain--full) .brain__svg {
        height: 220px;
    }

    .brain__node {
        cursor: pointer;
    }

    .brain__node--dim {
        opacity: 0.22;
    }

    .brain__node--focus circle {
        stroke: var(--color-accent);
        stroke-width: 3;
    }

    .brain__node--neighbor circle {
        stroke: var(--color-accent);
        stroke-width: 2;
    }

    .brain__label {
        font-family: var(--font-mono);
        font-size: 9px;
        fill: var(--color-text);
        pointer-events: none;
    }

    .brain__label--invert {
        fill: var(--color-surface);
    }

    .brain__caption {
        font-family: var(--font-mono);
        font-size: 11px;
        letter-spacing: 0.16em;
        fill: var(--color-text-muted);
    }

    /* ── Selected-node name (muted text, no card chrome) ── */
    .brain__name {
        position: absolute;
        transform: translateX(-50%);
        pointer-events: none;
        white-space: nowrap;
        font-family: var(--font-mono);
        font-size: 0.6875rem;
        color: var(--color-text-muted);
    }

    /* ── Expand affordance (preview) ── */
    .brain__expand {
        position: absolute;
        top: var(--space-2);
        right: var(--space-2);
        display: flex;
        padding: var(--space-1);
        background: var(--color-surface);
        border: 1px solid var(--color-border-subtle);
        border-radius: var(--radius-sm);
        color: var(--color-text-muted);
        cursor: pointer;
        transition: color 0.1s;
    }

    .brain__expand:hover {
        color: var(--color-text);
    }
</style>
