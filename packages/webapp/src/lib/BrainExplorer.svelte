<script lang="ts">
    import type { BrainModel, NeuronType } from "./brainModel";
    import { brainCounts } from "./brainModel";
    import { layoutBrain, type LaidOutEdge, type Point } from "./brainLayout";

    let {
        model,
        onClose,
    }: {
        model: BrainModel;
        onClose: () => void;
    } = $props();

    // Above this connection count we open with the weakest edges hidden so the
    // first paint is not a hairball; the slider reveals the rest.
    const ANTI_HAIRBALL_CONN = 200;
    const HIT_RADIUS = 12;

    const layout = $derived(layoutBrain(model));
    const counts = $derived(brainCounts(model));
    const neuronType = $derived(
        new Map<number, NeuronType>(model.neurons.map((n) => [n.id, n.type])),
    );
    const biasOf = $derived(
        new Map<number, number>(model.neurons.map((n) => [n.id, n.bias])),
    );
    const maxWeight = $derived(
        Math.max(1, ...model.connections.map((c) => Math.abs(c.weight))),
    );

    // Initial anti-hairball threshold: keep the ~ANTI_HAIRBALL_CONN strongest.
    const initialThreshold = $derived.by(() => {
        if (counts.connections <= ANTI_HAIRBALL_CONN) return 0;
        const sorted = model.connections
            .map((c) => Math.abs(c.weight))
            .sort((a, b) => b - a);
        return Math.round((sorted[ANTI_HAIRBALL_CONN] ?? 0) * 100) / 100;
    });

    // ── Reactive state ──
    let canvasEl = $state<HTMLCanvasElement | undefined>();
    let wrapW = $state(0);
    let wrapH = $state(0);
    let view = $state({ z: 1, px: 0, py: 0 });
    let focusId = $state<number | null>(null);
    let hover = $state<{ id: number; sx: number; sy: number } | null>(null);
    let threshold = $state(0);
    let filters = $state({
        sense: true,
        internal: true,
        action: true,
        recurrent: true,
    });
    let searchVal = $state("");
    let searchError = $state(false);

    // Resolved canvas colors, read from raw tokens (canvas needs concrete color
    // strings; semantic aliases resolve to "var(...)"). All values live in
    // tokens.css — never hardcoded here. See App.svelte for the same pattern.
    // Empty until readColors() runs; draw() short-circuits before first paint.
    let colors = $state({
        surface: "",
        ink: "",
        edge: "",
        dim: "",
        accent: "",
        muted: "",
    });

    function readColors() {
        const s = getComputedStyle(document.documentElement);
        const get = (name: string) => s.getPropertyValue(name).trim();
        colors = {
            surface: get("--_white"),
            ink: get("--_black"),
            edge: get("--_canvas-edge"),
            dim: get("--_canvas-dim"),
            accent: get("--_accent"),
            muted: get("--_neutral-500"),
        };
    }

    const scale0 = () => Math.min(wrapW, wrapH) * 0.9;
    const toScreenX = (x: number) =>
        (x - 0.5) * scale0() * view.z + wrapW / 2 + view.px;
    const toScreenY = (y: number) =>
        (y - 0.5) * scale0() * view.z + wrapH / 2 + view.py;

    // ── Focus + visibility ──
    const focusNeighbours = $derived.by(() => {
        if (focusId === null) return null;
        const set = new Set<number>([focusId]);
        for (const e of layout.edges) {
            if (e.source === focusId) set.add(e.sink);
            if (e.sink === focusId) set.add(e.source);
        }
        return set;
    });

    function edgeVisible(e: LaidOutEdge): boolean {
        if (Math.abs(e.weight) < threshold) return false;
        if (!filters.recurrent && e.routing !== "forward") return false;
        const s = neuronType.get(e.source);
        const t = neuronType.get(e.sink);
        if (!s || !t || !filters[s] || !filters[t]) return false;
        return true;
    }

    const nodeVisible = (id: number) => {
        const t = neuronType.get(id);
        return t ? filters[t] : false;
    };

    // ── Drawing ──
    function edgePath(ctx: CanvasRenderingContext2D, e: LaidOutEdge) {
        const a = layout.positions.get(e.source)!;
        const b = layout.positions.get(e.sink)!;
        const ax = toScreenX(a.x);
        const ay = toScreenY(a.y);
        if (e.routing === "self") {
            const r = 10 * view.z;
            ctx.moveTo(ax, ay - 4);
            ctx.arc(ax, ay, r, -Math.PI / 2, Math.PI / 2, false);
            return;
        }
        const bx = toScreenX(b.x);
        const by = toScreenY(b.y);
        const mx = (ax + bx) / 2;
        const my = (ay + by) / 2;
        const dx = bx - ax;
        const dy = by - ay;
        const len = Math.hypot(dx, dy) || 1;
        const off = e.bow * scale0() * view.z;
        ctx.moveTo(ax, ay);
        ctx.quadraticCurveTo(
            mx + (-dy / len) * off,
            my + (dx / len) * off,
            bx,
            by,
        );
    }

    function drawEdges(ctx: CanvasRenderingContext2D) {
        const fset = focusNeighbours;
        for (const e of layout.edges) {
            if (!edgeVisible(e)) continue;
            const connected =
                fset === null || e.source === focusId || e.sink === focusId;
            ctx.beginPath();
            ctx.lineWidth = Math.max(0.5, (Math.abs(e.weight) / maxWeight) * 3);
            ctx.strokeStyle =
                fset === null
                    ? colors.edge
                    : connected
                      ? colors.accent
                      : colors.dim;
            ctx.setLineDash(e.weight < 0 ? [4, 3] : []);
            edgePath(ctx, e);
            ctx.stroke();
        }
        ctx.setLineDash([]);
    }

    function nodeStyle(id: number): { stroke: string; fill: string } {
        const fset = focusNeighbours;
        if (fset && !fset.has(id))
            return { stroke: colors.dim, fill: colors.surface };
        const focused = fset !== null;
        return {
            stroke: focused ? colors.accent : colors.ink,
            fill: colors.surface,
        };
    }

    function drawNode(
        ctx: CanvasRenderingContext2D,
        id: number,
        type: NeuronType,
        p: Point,
    ) {
        const x = toScreenX(p.x);
        const y = toScreenY(p.y);
        const r = type === "internal" ? 3.5 : 5;
        const { stroke, fill } = nodeStyle(id);
        ctx.beginPath();
        ctx.arc(x, y, r, 0, Math.PI * 2);
        if (type === "action") {
            // Solid dot.
            ctx.fillStyle = stroke;
            ctx.fill();
        } else {
            // Hollow ring (sense) / small ring (internal).
            ctx.fillStyle = fill;
            ctx.fill();
            ctx.lineWidth = 1.2;
            ctx.strokeStyle = stroke;
            ctx.stroke();
        }
    }

    function draw() {
        if (!canvasEl || wrapW === 0 || wrapH === 0) return;
        const ctx = canvasEl.getContext("2d");
        if (!ctx) return;
        const dpr = window.devicePixelRatio || 1;
        canvasEl!.width = wrapW * dpr;
        canvasEl!.height = wrapH * dpr;
        ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
        ctx.fillStyle = colors.surface;
        ctx.fillRect(0, 0, wrapW, wrapH);
        drawEdges(ctx);
        for (const n of model.neurons) {
            if (!nodeVisible(n.id)) continue;
            drawNode(ctx, n.id, n.type, layout.positions.get(n.id)!);
        }
    }

    // Redraw whenever any visual input changes. draw() reads view, focusId,
    // threshold, filters, colors, dims and layout, so all are tracked here.
    $effect(() => {
        draw();
    });

    $effect(() => {
        readColors();
    });

    // Apply the anti-hairball default when the model (and thus its initial
    // threshold) changes; user slider edits afterwards are preserved.
    $effect(() => {
        threshold = initialThreshold;
    });

    // ── Pointer interaction ──
    let dragging = false;
    let dragMoved = false;
    let lastX = 0;
    let lastY = 0;

    function hitTest(sx: number, sy: number): number | null {
        let best: number | null = null;
        let bestD = HIT_RADIUS;
        for (const n of model.neurons) {
            if (!nodeVisible(n.id)) continue;
            const p = layout.positions.get(n.id)!;
            const d = Math.hypot(toScreenX(p.x) - sx, toScreenY(p.y) - sy);
            if (d < bestD) {
                bestD = d;
                best = n.id;
            }
        }
        return best;
    }

    function localCoords(e: PointerEvent): { x: number; y: number } {
        const rect = canvasEl!.getBoundingClientRect();
        return { x: e.clientX - rect.left, y: e.clientY - rect.top };
    }

    function onPointerDown(e: PointerEvent) {
        dragging = true;
        dragMoved = false;
        lastX = e.clientX;
        lastY = e.clientY;
        canvasEl!.setPointerCapture(e.pointerId);
    }

    function onPointerMove(e: PointerEvent) {
        const { x, y } = localCoords(e);
        if (dragging) {
            const dx = e.clientX - lastX;
            const dy = e.clientY - lastY;
            if (Math.abs(dx) + Math.abs(dy) > 2) dragMoved = true;
            view = { ...view, px: view.px + dx, py: view.py + dy };
            lastX = e.clientX;
            lastY = e.clientY;
            return;
        }
        const id = hitTest(x, y);
        hover = id === null ? null : { id, sx: x, sy: y };
    }

    function onPointerUp(e: PointerEvent) {
        if (dragging && !dragMoved) {
            const { x, y } = localCoords(e);
            const id = hitTest(x, y);
            focusId = id === focusId ? null : id;
        }
        dragging = false;
        canvasEl?.releasePointerCapture(e.pointerId);
    }

    function onWheel(e: WheelEvent) {
        e.preventDefault();
        const rect = canvasEl!.getBoundingClientRect();
        const mx = e.clientX - rect.left;
        const my = e.clientY - rect.top;
        // World point under cursor before zoom.
        const s0 = scale0();
        const wx = (mx - wrapW / 2 - view.px) / (s0 * view.z) + 0.5;
        const wy = (my - wrapH / 2 - view.py) / (s0 * view.z) + 0.5;
        const z = Math.min(
            8,
            Math.max(0.3, view.z * (e.deltaY < 0 ? 1.1 : 0.9)),
        );
        // Keep the same world point under the cursor.
        const px = mx - wrapW / 2 - (wx - 0.5) * s0 * z;
        const py = my - wrapH / 2 - (wy - 0.5) * s0 * z;
        view = { z, px, py };
    }

    function fitToView() {
        view = { z: 1, px: 0, py: 0 };
    }

    function jumpToNeuron() {
        const id = parseInt(searchVal, 10);
        const p = layout.positions.get(id);
        if (isNaN(id) || !p) {
            searchError = true;
            return;
        }
        searchError = false;
        focusId = id;
        const z = Math.max(view.z, 2);
        view = {
            z,
            px: -(p.x - 0.5) * scale0() * z,
            py: -(p.y - 0.5) * scale0() * z,
        };
    }

    function onKeydown(e: KeyboardEvent) {
        if (e.key === "Escape") onClose();
    }

    const hoverInfo = $derived.by(() => {
        if (!hover) return null;
        return {
            id: hover.id,
            type: neuronType.get(hover.id) ?? "?",
            bias: biasOf.get(hover.id) ?? 0,
            sx: hover.sx,
            sy: hover.sy,
        };
    });

    const isDense = $derived(counts.connections > ANTI_HAIRBALL_CONN);
</script>

<svelte:window onkeydown={onKeydown} />

<div
    class="explorer"
    role="dialog"
    aria-modal="true"
    aria-label="Brain explorer"
>
    <header class="explorer__bar">
        <span class="panel__title">Brain explorer</span>
        <span class="explorer__counts">
            {counts.sense} IN · {counts.internal} HIDDEN · {counts.action} OUT ·
            {counts.connections} CONN
        </span>
        <span class="explorer__spacer"></span>
        <button class="button button--ghost" onclick={fitToView}>fit</button>
        <button
            class="button button--icon"
            onclick={onClose}
            aria-label="Close explorer"
        >
            ✕
        </button>
    </header>

    <div class="explorer__body">
        <div
            class="explorer__canvas-wrap"
            bind:clientWidth={wrapW}
            bind:clientHeight={wrapH}
        >
            <canvas
                bind:this={canvasEl}
                class="explorer__canvas"
                onpointerdown={onPointerDown}
                onpointermove={onPointerMove}
                onpointerup={onPointerUp}
                onwheel={onWheel}
            ></canvas>

            {#if hoverInfo}
                <div
                    class="explorer__tooltip"
                    style="left: {hoverInfo.sx + 12}px; top: {hoverInfo.sy +
                        12}px"
                >
                    <span class="explorer__tip-id">#{hoverInfo.id}</span>
                    {hoverInfo.type} · bias {hoverInfo.bias.toFixed(2)}
                </div>
            {/if}
        </div>

        <aside class="explorer__controls">
            <div class="field-row">
                <label class="field-label" for="brain-search">
                    jump to neuron
                </label>
                <input
                    id="brain-search"
                    class="control"
                    class:explorer__search--err={searchError}
                    type="number"
                    min="0"
                    placeholder="id"
                    bind:value={searchVal}
                    onkeydown={(e) => e.key === "Enter" && jumpToNeuron()}
                />
                {#if searchError}
                    <span class="field-hint">no such neuron</span>
                {/if}
            </div>

            <div class="field-row">
                <label class="field-label" for="brain-threshold">
                    weight threshold · {threshold.toFixed(2)}
                </label>
                <input
                    id="brain-threshold"
                    type="range"
                    min="0"
                    max={maxWeight}
                    step="0.05"
                    bind:value={threshold}
                />
                {#if isDense}
                    <span class="field-hint">
                        dense brain — weakest edges hidden; drag to reveal.
                    </span>
                {/if}
            </div>

            <div class="field-row">
                <span class="field-label">filters</span>
                <label class="field-row toggle-row toggle-label">
                    <input
                        class="toggle-checkbox"
                        type="checkbox"
                        name="filter-sense"
                        bind:checked={filters.sense}
                    />
                    <span class="field-label">sense</span>
                </label>
                <label class="field-row toggle-row toggle-label">
                    <input
                        class="toggle-checkbox"
                        type="checkbox"
                        name="filter-internal"
                        bind:checked={filters.internal}
                    />
                    <span class="field-label">internal</span>
                </label>
                <label class="field-row toggle-row toggle-label">
                    <input
                        class="toggle-checkbox"
                        type="checkbox"
                        name="filter-action"
                        bind:checked={filters.action}
                    />
                    <span class="field-label">action</span>
                </label>
                <label class="field-row toggle-row toggle-label">
                    <input
                        class="toggle-checkbox"
                        type="checkbox"
                        name="filter-recurrent"
                        bind:checked={filters.recurrent}
                    />
                    <span class="field-label">recurrent edges</span>
                </label>
            </div>

            <p class="field-hint explorer__legend">
                ring = sense · small ring = internal · dot = action<br />
                solid = excitatory · dashed = inhibitory · width ∝ |weight|<br
                />
                click a neuron to focus · Esc to close
            </p>
        </aside>
    </div>
</div>

<style>
    .explorer {
        position: fixed;
        inset: 0;
        z-index: 40;
        display: flex;
        flex-direction: column;
        background: var(--color-surface);
    }

    .explorer__bar {
        display: flex;
        align-items: center;
        gap: var(--space-3);
        padding: var(--space-3) var(--space-4);
        border-bottom: 1px solid var(--color-border-subtle);
    }

    .explorer__counts {
        font-family: var(--font-mono);
        font-size: var(--text-sm);
        color: var(--color-text-muted);
    }

    .explorer__spacer {
        flex: 1;
    }

    .explorer__body {
        flex: 1;
        display: flex;
        min-height: 0;
    }

    .explorer__canvas-wrap {
        position: relative;
        flex: 1;
        min-width: 0;
    }

    .explorer__canvas {
        position: absolute;
        inset: 0;
        width: 100%;
        height: 100%;
        display: block;
        touch-action: none;
        cursor: grab;
    }

    .explorer__tooltip {
        position: absolute;
        pointer-events: none;
        font-family: var(--font-mono);
        font-size: 0.6875rem;
        color: var(--color-text);
        background: var(--color-surface-glass);
        border: 1px solid var(--color-border-subtle);
        border-radius: var(--radius-sm);
        padding: var(--space-1) var(--space-2);
        white-space: nowrap;
        box-shadow: var(--shadow-floating);
    }

    .explorer__tip-id {
        color: var(--color-accent-text);
    }

    .explorer__controls {
        width: 16rem;
        flex-shrink: 0;
        padding: var(--space-4);
        border-left: 1px solid var(--color-border-subtle);
        overflow-y: auto;
    }

    .explorer__search--err {
        border-color: var(--color-warn);
    }

    .explorer__legend {
        margin-top: var(--space-6);
        line-height: 1.7;
    }
</style>
