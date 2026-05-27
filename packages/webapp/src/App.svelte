<script lang="ts">
    import type { WorkerCmd, WorkerEvent } from "./workers/sim.worker";
    import TopBar from "./lib/TopBar.svelte";
    import PlayDock from "./lib/PlayDock.svelte";
    import GridView from "./lib/GridView.svelte";
    import HUD from "./lib/HUD.svelte";
    import RightRail from "./lib/RightRail.svelte";
    import SimConfigPanel from "./lib/SimConfigPanel.svelte";
    import CellPanel from "./lib/CellPanel.svelte";

    // ── Canvas / worker ──────────────────────────────────────────────────────
    let canvasEl = $state<HTMLCanvasElement | undefined>();
    let workerReady = $state(false);

    const worker = new Worker(
        new URL("./workers/sim.worker.ts", import.meta.url),
        { type: "module" },
    );

    worker.addEventListener("message", (e: MessageEvent<WorkerEvent>) => {
        const msg = e.data;
        if (msg.type === "ready") {
            if (canvasEl) {
                const offscreen = canvasEl.transferControlToOffscreen();
                worker.postMessage(
                    { type: "canvas", canvas: offscreen } satisfies WorkerCmd,
                    [offscreen],
                );
                workerReady = true;
                // Send initial layout so the worker sizes the canvas and knows
                // where the grid region lives on the full-viewport surface.
                send({
                    type: "layout",
                    canvasW: viewportW,
                    canvasH: viewportH,
                    gridX: gridGeom.x,
                    gridY: gridGeom.y,
                    gridSize: gridGeom.size,
                    gridCells: gridSize,
                } satisfies WorkerCmd);
            }
        } else if (msg.type === "status") {
            // Parse step count from "Run step N" status messages.
            const m = msg.message.match(/Run step (\d+)/);
            if (m) currentStep = parseInt(m[1], 10);
            // Detect auto-stop at end of generation.
            if (msg.message.includes("Reached end of generation")) {
                isRunning = false;
            }
        } else if (msg.type === "census") {
            currentGen = msg.gen;
            currentPop = msg.population;
            const rate =
                msg.population > 0 ? msg.survivors / msg.population : 0;
            survivalHistory = [...survivalHistory.slice(-11), rate];
        } else if (msg.type === "configured") {
            isRunning = false;
            currentGen = 0;
            currentStep = 0;
            survivalHistory = [];
            currentPop = msg.population;
            gridSize = msg.gridSizeX;
            stepsPerGen = msg.stepsPerGen;
        }
        // "error" type is silently ignored in this phase; no UI for it yet.
    });

    function send(cmd: WorkerCmd): void {
        worker.postMessage(cmd);
    }

    // ── Simulation state ─────────────────────────────────────────────────────
    let isRunning = $state(false);
    let currentGen = $state(0);
    let currentStep = $state(0);
    let currentPop = $state(3000);
    let survivalHistory = $state<number[]>([]);

    // These reflect the active simulation parameters and are updated when the
    // config panel applies a new configuration.
    let stepsPerGen = $state(300);
    let gridSize = $state(128);

    // ── UI state ─────────────────────────────────────────────────────────────
    let selectedAgentId = $state<number | null>(null);
    let railOpen = $state(false);
    let activeTab = $state<"sim" | "cell">("sim");

    // Open Cell tab automatically when an agent is selected.
    $effect(() => {
        if (selectedAgentId !== null) {
            railOpen = true;
            activeTab = "cell";
        }
    });

    // ── Viewport & grid geometry ─────────────────────────────────────────────
    let viewportW = $state(window.innerWidth);
    let viewportH = $state(window.innerHeight);

    $effect(() => {
        function onResize(): void {
            viewportW = window.innerWidth;
            viewportH = window.innerHeight;
        }
        window.addEventListener("resize", onResize);
        return () => window.removeEventListener("resize", onResize);
    });

    const PAD_TOP = 80;
    const PAD_SIDE = 80;
    const PAD_BOTTOM = 180;
    const TOPBAR_H = 56; // 3.5rem at 16px
    const RAIL_W = 380;

    const gridGeom = $derived.by(() => {
        const railW = railOpen && viewportW > 760 ? RAIL_W : 0;
        const availW = viewportW - railW - PAD_SIDE * 2;
        const availH = viewportH - TOPBAR_H - PAD_TOP - PAD_BOTTOM;
        const size = Math.max(140, Math.min(availW, availH, 760));
        const x = PAD_SIDE + (availW - size) / 2;
        const y = TOPBAR_H + PAD_TOP + (availH - size) / 2;
        return { x, y, size, cx: x + size / 2, cy: y + size / 2 };
    });

    // Re-send layout whenever the grid geometry or viewport dims change so the
    // worker can resize the canvas and reposition the grid region.
    $effect(() => {
        if (!workerReady) return;
        send({
            type: "layout",
            canvasW: viewportW,
            canvasH: viewportH,
            gridX: gridGeom.x,
            gridY: gridGeom.y,
            gridSize: gridGeom.size,
            gridCells: gridSize,
        } satisfies WorkerCmd);
    });

    // ── Play / pause / step / gen / reset ────────────────────────────────────
    function handleToggle(): void {
        if (isRunning) {
            isRunning = false;
            send({ type: "stop" });
        } else {
            isRunning = true;
            send({ type: "play" });
        }
    }

    function handleStep(): void {
        send({ type: "step" });
    }

    function handleGen(): void {
        send({ type: "nextGeneration" });
    }

    function handleReset(): void {
        isRunning = false;
        // 'reset' returns the worker to idle (kinematic sculpture) and zeros
        // playing state; 'stop' alone would only freeze agents at grid positions.
        send({ type: "reset" });
        currentGen = 0;
        currentStep = 0;
        survivalHistory = [];
    }

    // ── Agent selection ───────────────────────────────────────────────────────
    function handleClearSelection(): void {
        selectedAgentId = null;
        activeTab = "sim";
    }
</script>

<div class="app-shell">
    <!--
        Full-viewport canvas — sits at z-index 0.
        The worker renders kinetic sculpture + agents + grid interior on this surface.
        The grid is a REGION of this canvas, not a separate element.
    -->
    <canvas bind:this={canvasEl}></canvas>

    <!-- z-index: 20 — fixed top bar -->
    <TopBar
        running={isRunning}
        gen={currentGen}
        step={currentStep}
        {stepsPerGen}
    />

    <!-- z-index: 25 — floating play controls, centered over the grid -->
    <PlayDock
        running={isRunning}
        centerX={gridGeom.cx}
        onToggle={handleToggle}
        onStep={handleStep}
        onGen={handleGen}
        onReset={handleReset}
    />

    <!-- z-index: 5 — transparent overlay: crop marks + idle text only -->
    <GridView geom={gridGeom} running={isRunning} {gridSize} />

    <!-- z-index: 15 — telemetry HUD, bottom-left -->
    <HUD
        running={isRunning}
        gen={currentGen}
        step={currentStep}
        {stepsPerGen}
        pop={currentPop}
        {survivalHistory}
    />

    <!-- Hamburger toggle — top-right, z-index: 30 -->
    <button
        class="hamburger"
        onclick={() => (railOpen = !railOpen)}
        aria-label={railOpen ? "Close panel" : "Open panel"}
        aria-expanded={railOpen}
    >
        <svg
            width="18"
            height="14"
            viewBox="0 0 18 14"
            fill="none"
            stroke="currentColor"
            stroke-width="1.5"
            stroke-linecap="round"
            aria-hidden="true"
        >
            <line x1="0" y1="1" x2="18" y2="1" />
            <line x1="0" y1="7" x2="18" y2="7" />
            <line x1="0" y1="13" x2="18" y2="13" />
        </svg>
    </button>

    <!-- z-index: 22 — slide-in right rail with two tabs -->
    <RightRail
        open={railOpen}
        {activeTab}
        hasSelection={selectedAgentId !== null}
        onTabChange={(t) => (activeTab = t)}
    >
        {#snippet sim()}<SimConfigPanel {send} />{/snippet}
        {#snippet cell()}
            <CellPanel agent={null} onClear={handleClearSelection} />
        {/snippet}
    </RightRail>
</div>

<style>
    .app-shell {
        position: fixed;
        inset: 0;
        overflow: hidden;
        background-color: var(--color-surface);
        background-image: radial-gradient(
            var(--color-dot-grid) 1px,
            transparent 1px
        );
        background-size: 18px 18px;
    }

    canvas {
        position: absolute;
        inset: 0;
        width: 100%;
        height: 100%;
        z-index: 0;
        display: block;
    }

    .hamburger {
        position: fixed;
        right: var(--space-6);
        top: calc(3.5rem + var(--space-5));
        z-index: 30;
        width: 2.75rem;
        height: 2.75rem;
        display: flex;
        align-items: center;
        justify-content: center;
        background: var(--color-surface);
        border: 1px solid var(--color-border);
        border-radius: var(--radius-sm);
        cursor: pointer;
        color: var(--color-text);
        transition:
            border-color 0.1s,
            color 0.1s;
    }

    .hamburger:hover {
        border-color: var(--color-accent);
        color: var(--color-accent-text);
    }

    .hamburger:focus-visible {
        outline: 2px solid var(--color-accent);
        outline-offset: 2px;
    }
</style>
