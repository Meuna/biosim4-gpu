<script lang="ts">
    import type {
        WorkerCmd,
        WorkerEvent,
        SimParams,
    } from "./workers/sim.worker";
    import TopBar from "./lib/TopBar.svelte";
    import GridView from "./lib/GridView.svelte";
    import SurvivalSparkline from "./lib/SurvivalSparkline.svelte";
    import TelemetryHUD from "./lib/TelemetryHUD.svelte";
    import RightRail from "./lib/RightRail.svelte";
    import SimConfigPanel from "./lib/SimConfigPanel.svelte";
    import CellPanel from "./lib/CellPanel.svelte";
    import HoverCard from "./lib/HoverCard.svelte";
    import BrainExplorer from "./lib/BrainExplorer.svelte";
    import ConfigChangeDialog from "./lib/ConfigChangeDialog.svelte";
    import EvolveOverlay from "./lib/EvolveOverlay.svelte";
    import ErrorBanner from "./lib/ErrorBanner.svelte";
    import FatalErrorOverlay from "./lib/FatalErrorOverlay.svelte";
    import type { BrainConn } from "./lib/brain";
    import { MousePointerClick, ArrowLeft, Menu } from "lucide-svelte";
    import { simParamsToToml, tomlToSimParams } from "./lib/tomlConfig";
    import {
        pickFile,
        downloadBlob,
        classifyDroppedFiles,
    } from "./lib/fileTransfer";
    import { shouldSpaceBeat } from "./lib/beatTrigger";
    import { SimMachine } from "./lib/simMachine.svelte";
    import { AgentFocus } from "./lib/agentFocus.svelte";
    import { SimTelemetry } from "./lib/simTelemetry.svelte";
    import { computeGridGeom, hudBounds, hamburgerInset } from "./lib/gridGeom";
    import { configForFormFactor, detectFormFactor } from "./lib/formFactor";

    // ── Canvas / worker ──────────────────────────────────────────────────────
    let canvasEl = $state<HTMLCanvasElement | undefined>();

    // Render mode is owned by the worker (sole authority); the main thread only
    // observes it via the "renderMode" event to toggle the idle overlay and
    // (sub-plan C) route canvas clicks. Defaults to the idle sculpture.
    let renderMode = $state<"kinematic" | "grid">("kinematic");

    const worker = new Worker(
        new URL("./workers/sim.worker.ts", import.meta.url),
        { type: "module" },
    );

    // ── Simulation state machine ─────────────────────────────────────────────
    // Owns { phase, dirty }, the draft/last-played configs, the genome
    // compatibility gate, and every lifecycle worker command. UX handlers and
    // worker replies below delegate to it; the rest of this component only
    // reads its getters.
    // The form factor is classified once, here at construction, to seed the
    // initial draft/last-played config. This is a one-time startup choice, not
    // reactive: it never re-fires on resize/rotation and never clobbers a draft
    // the user has edited. A later TOML import still overrides it.
    const machine = new SimMachine(
        (cmd, transfer) => worker.postMessage(cmd, transfer ?? []),
        configForFormFactor(detectFormFactor()),
    );

    // ── Agent-focus controller ───────────────────────────────────────────────
    // Owns the focus triad { selected, hovered, lastHovered }, the derived
    // display agent, and every agent-related worker command. Worker replies and
    // UX gestures below delegate to it.
    const focus = new AgentFocus(send);

    // ── Simulation-telemetry holder ──────────────────────────────────────────
    // Owns the display-only counters (gen/step/pop), the survival sparkline,
    // the active grid/steps parameters, and the snapshot-ready gate — all
    // derived purely from worker replies. The third sibling alongside
    // `SimMachine` and `AgentFocus`; it issues no commands, so takes no `send`.
    const telemetry = new SimTelemetry();

    const workerReady = $derived(machine.phase !== "WORKER_PENDING");

    // Transfers the canvas to the worker and hands it the initial layout. Stays
    // in-component: it closes over `canvasEl`, the viewport dims and `gridGeom`.
    function bootstrapCanvas(): void {
        if (!canvasEl) return;
        const offscreen = canvasEl.transferControlToOffscreen();
        // Read raw tokens directly (not via var() aliases) so getPropertyValue
        // resolves to usable color strings.
        const styles = getComputedStyle(document.documentElement);
        const overlayColor = styles.getPropertyValue("--_border-subtle").trim();
        const borderColor = styles
            .getPropertyValue("--_challenge-border")
            .trim();
        const accentColor = styles.getPropertyValue("--_accent").trim();
        worker.postMessage(
            {
                type: "canvas",
                canvas: offscreen,
                overlayColor,
                borderColor,
                accentColor,
            } satisfies WorkerCmd,
            [offscreen],
        );
        machine.onWorkerReady();
        // Send initial layout so the worker sizes the canvas and knows where the
        // grid region lives on the full-viewport surface.
        send({
            type: "layout",
            canvasW: viewportW,
            canvasH: viewportH,
            gridX: gridGeom.x,
            gridY: gridGeom.y,
            gridW: gridGeom.w,
            gridH: gridGeom.h,
        } satisfies WorkerCmd);
    }

    // Routes worker replies to their owners. Ordered by lifecycle: bootstrap,
    // then (re)configuration, per-step progress, agent focus, then misc.
    worker.addEventListener("message", (e: MessageEvent<WorkerEvent>) => {
        const msg = e.data;
        switch (msg.type) {
            case "ready":
                bootstrapCanvas();
                break;
            case "renderMode":
                renderMode = msg.mode;
                break;
            case "configured":
                machine.onConfigured();
                telemetry.onConfigured(msg);
                break;
            case "census":
                machine.onCensus(msg.requiredGenomeLen, msg.requiredNeurons);
                telemetry.onCensus(msg);
                break;
            case "rewindConfigured":
                machine.onRewindConfigured();
                telemetry.onRewindConfigured(msg);
                break;
            case "nextGenerationConfigured":
                machine.onNextGenerationConfigured(
                    msg.requiredGenomeLen,
                    msg.requiredNeurons,
                );
                telemetry.onNextGenerationConfigured(msg);
                break;
            case "snapshotLoaded":
                machine.onSnapshotLoaded(
                    msg.requiredGenomeLen,
                    msg.requiredNeurons,
                );
                telemetry.onSnapshotLoaded(msg);
                // Surface the config panel so the incompatible-field hint
                // explains why the lifecycle buttons greyed out.
                if (machine.genomIncompatible) {
                    railOpen = true;
                    activeTab = "sim";
                }
                break;
            case "stepped":
                telemetry.onStepped(msg);
                break;
            case "genComplete":
                telemetry.onStepped(msg);
                machine.onGenComplete();
                break;
            case "paused":
                telemetry.onStepped(msg);
                machine.onFreeRunPaused();
                break;
            // Agent selection — a 4-leg round-trip with a single-writer rule:
            //   1. canvas click/hover → send pickAgentAtCell{gx,gy,reason}
            //      (only the main thread knows pixel→cell geometry)
            //   2. worker reads the WASM heap → agentPicked{info,reason} |
            //      agentMissed{reason} (only the worker can read the heap)
            //   3. focus.pick/miss → send selectAgent|hoverAgent{id} — the SOLE
            //      writer of the worker's selectedAgentId/hoveredAgentId
            //   4. each step → agentUpdated{info} feeds focus.update (selected)
            // `AgentFocus` is the sole authority on the selected/hovered/sticky
            // triad (see agentFocus.svelte.ts); the worker only mirrors which id
            // to highlight and feed. Keeping selection writes on leg 3 alone is
            // why the pick reply does not set worker state directly.
            case "agentPicked":
                focus.pick(msg.info, msg.reason);
                break;
            case "agentMissed":
                focus.miss(msg.reason);
                break;
            case "agentUpdated":
                focus.update(msg.info);
                break;
            case "brainData":
                // Ignore brain payloads for an agent no longer displayed.
                if (msg.id === focus.displayId) {
                    brain = {
                        id: msg.id,
                        conns: msg.conns,
                        neuronCount: msg.neuronCount,
                    };
                }
                break;
            case "fps":
                measuredFps = msg.value;
                break;
            case "snapshotData":
                downloadBlob(
                    "biosim.snap",
                    msg.data,
                    "application/octet-stream",
                );
                break;
            case "error":
                if (msg.fatal) fatalError = msg.message;
                else bannerError = msg.message;
                break;
        }
    });

    function send(cmd: WorkerCmd): void {
        worker.postMessage(cmd);
    }

    // Telemetry (gen/step/pop counters, survival sparkline, active grid/steps
    // params, snapshot-ready gate) lives in the `telemetry` holder above.

    // ── Speed / FPS ──────────────────────────────────────────────────────────
    let targetSpeed = $state(0);
    let measuredFps = $state<number | null>(null);

    $effect(() => {
        if (machine.phase !== "STEPS_RUNNING") {
            measuredFps = null;
        }
    });

    // ── UI state ─────────────────────────────────────────────────────────────
    // The agent-focus triad and its derived display agent live in `focus`
    // (AgentFocus); this component only reads its getters.
    let mouseX = $state(0);
    let mouseY = $state(0);
    let railOpen = $state(false);
    let activeTab = $state<"sim" | "cell">("sim");

    // ── Brain explorer ───────────────────────────────────────────────────────
    let brain = $state<{
        id: number;
        conns: BrainConn[];
        neuronCount: number;
    } | null>(null);
    let brainExpanded = $state(false);
    // Last agent id we asked the worker for, so we request once per selection —
    // not on every reactive change of `displayAgent` (it is reassigned each step
    // by the live-update feed, but the brain topology is fixed for the agent).
    let requestedBrainId: number | null = null;

    // Request the brain for the currently displayed agent. Does NOT read `brain`
    // (writing it from the brainData handler would otherwise re-trigger this
    // effect into a request loop); `displayBrain` gates stale data instead.
    $effect(() => {
        const id = focus.displayId;
        if (id === null || !workerReady) {
            requestedBrainId = null;
            return;
        }
        if (id === requestedBrainId) return;
        requestedBrainId = id;
        send({ type: "requestBrain", id });
    });

    // Close the full-screen explorer on Escape.
    $effect(() => {
        function onKey(e: KeyboardEvent): void {
            if (e.key === "Escape" && brainExpanded) brainExpanded = false;
        }
        window.addEventListener("keydown", onKey);
        return () => window.removeEventListener("keydown", onKey);
    });

    // The brain payload aligned with the currently displayed agent (or null).
    const displayBrain = $derived(
        brain && brain.id === focus.displayId ? brain : null,
    );

    // Open the rail on the Cell tab whenever the displayed agent changes to a
    // distinct one — including after a manual close. Tracking the previous id
    // prevents re-firing on every agentUpdated live-update message while the
    // same agent stays displayed. Reads `focus` (controller state) and writes
    // only view state, so it cannot self-trigger.
    let prevDisplayAgentId: number | null = null;
    $effect(() => {
        const id = focus.displayId;
        if (id !== null && id !== prevDisplayAgentId) {
            railOpen = true;
            activeTab = "cell";
        }
        prevDisplayAgentId = id;
    });

    // Clear hover state when Ctrl key is released.
    $effect(() => {
        function onKeyUp(e: KeyboardEvent): void {
            if (e.key === "Control") focus.clearHover();
        }
        window.addEventListener("keyup", onKeyUp);
        return () => window.removeEventListener("keyup", onKeyUp);
    });

    // Space pulses the idle sculpture full-viewport. Gated to sculpture mode and
    // away from editable/interactive focus (shouldSpaceBeat), so grid-mode space
    // and space-activates-button keep working; e.repeat is ignored for discrete
    // pulses (held space would otherwise pin the surface at peak).
    $effect(() => {
        function onKey(e: KeyboardEvent): void {
            if (e.repeat) return;
            if (!shouldSpaceBeat(e.key, renderMode, e.target)) return;
            e.preventDefault();
            send({ type: "beat" });
        }
        window.addEventListener("keydown", onKey);
        return () => window.removeEventListener("keydown", onKey);
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

    // Measured live from TopBar (bind:headerHeight); 56 = 3.5rem fallback until
    // the first measure. Grows when the header wraps to multiple rows.
    let topbarH = $state(56);

    const gridGeom = $derived(
        computeGridGeom({
            viewportW,
            viewportH,
            topbarH,
            railOpen,
            gridSizeX: telemetry.gridSizeX,
            gridSizeY: telemetry.gridSizeY,
        }),
    );

    // Telemetry sits right of the grid normally; below it once the viewport is
    // narrow enough that a right-placed block would clip. The grid is centred,
    // so its right edge is ~viewportW/2 + w/2 and telemetry-right clips below
    // ~1084px at max grid size — so switch at 1160, the header's 2-row reflow
    // breakpoint (TopBar.svelte), which clears that threshold. var() can't be
    // used in this comparison, so 1160 is duplicated here as in the header media.
    const telemetryPlacement = $derived(viewportW < 1160 ? "below" : "right");

    // Survival sparkline shares the grid's responsive width rules: it starts at
    // the shrinking side padding and is pushed in by the rail.
    const survivalBounds = $derived(hudBounds(viewportW, railOpen));

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
            gridW: gridGeom.w,
            gridH: gridGeom.h,
        } satisfies WorkerCmd);
    });

    // ── Play / pause / step / gen ────────────────────────────────────────────
    function handleSetSpeed(fps: number): void {
        targetSpeed = fps;
        send({ type: "setSpeed", fps } satisfies WorkerCmd);
    }

    function handleNextGen(autoPlay: boolean): void {
        machine.nextGen(autoPlay);
        if (!autoPlay) telemetry.resetStep();
    }

    function handleRewind(autoPlay: boolean): void {
        machine.rewind(autoPlay);
        if (!autoPlay) telemetry.resetStep();
    }

    // ── Agent selection ───────────────────────────────────────────────────────
    function handleClearSelection(): void {
        focus.clearSelection();
        activeTab = "sim";
        brainExpanded = false;
    }

    function pixelToCell(
        px: number,
        py: number,
    ): { gx: number; gy: number } | null {
        if (px < gridGeom.x || px >= gridGeom.x + gridGeom.w) return null;
        if (py < gridGeom.y || py >= gridGeom.y + gridGeom.h) return null;
        return {
            gx: Math.floor(
                (px - gridGeom.x) / (gridGeom.w / telemetry.gridSizeX),
            ),
            gy: Math.floor(
                (py - gridGeom.y) / (gridGeom.h / telemetry.gridSizeY),
            ),
        };
    }

    function handleCanvasClick(e: MouseEvent): void {
        if (!workerReady) return;
        // In sculpture mode a click is a beat decaying from the cursor; in grid
        // mode it picks an agent. The two paths never both fire.
        if (renderMode === "kinematic") {
            send({ type: "beat", x: e.clientX, y: e.clientY });
            return;
        }
        if (e.ctrlKey && focus.promoteHoverToSelection()) return;
        const cell = pixelToCell(e.clientX, e.clientY);
        if (cell) send({ type: "pickAgentAtCell", ...cell, reason: "click" });
    }

    let hoverRafPending = false;
    function handleCanvasHover(e: MouseEvent): void {
        mouseX = e.clientX;
        mouseY = e.clientY;
        if (!e.ctrlKey || !workerReady) {
            focus.endHover();
            return;
        }
        // The canvas is full-viewport, so onmouseleave only fires leaving the
        // whole surface — moving off the grid into the surrounding padding does
        // not. Compute the cell synchronously (cheap) and end the live hover the
        // moment the cursor leaves the grid region. This both hides the floating
        // card and demotes the hover to a sticky last-hover, so a follow-up
        // Ctrl+click can promote it. Only the worker `send` needs rAF throttling.
        const cell = pixelToCell(e.clientX, e.clientY);
        if (!cell) {
            focus.endHover();
            return;
        }
        if (hoverRafPending) return;
        hoverRafPending = true;
        requestAnimationFrame(() => {
            hoverRafPending = false;
            send({ type: "pickAgentAtCell", ...cell, reason: "hover" });
        });
    }

    // ── Drag-drop overlay ────────────────────────────────────────────────────
    let isDragging = $state(false);

    function handleDragEnter(e: DragEvent): void {
        if (!e.dataTransfer?.types.includes("Files")) return;
        isDragging = true;
    }

    // ── Error reporting ──────────────────────────────────────────────────────
    // `bannerError` drives the auto-dismissing bottom banner; `fatalError` drives
    // the blocking overlay for unrecoverable failures (e.g. bootstrap init) and
    // never auto-clears.
    let bannerError = $state<string | null>(null);
    let fatalError = $state<string | null>(null);

    $effect(() => {
        if (!bannerError) return;
        const t = setTimeout(() => {
            bannerError = null;
        }, 4000);
        return () => clearTimeout(t);
    });

    function handleConfDownload(): void {
        const toml = simParamsToToml(
            $state.snapshot(machine.draftConfig) as SimParams,
        );
        downloadBlob("biosim.toml", toml, "text/plain");
    }

    // Copies the draft config TOML to the clipboard, returning whether it
    // succeeded so the panel can flash a confirmation.
    async function handleConfCopy(): Promise<boolean> {
        const toml = simParamsToToml(
            $state.snapshot(machine.draftConfig) as SimParams,
        );
        if (!navigator.clipboard) {
            bannerError = "Clipboard API not available";
            return false;
        }
        try {
            await navigator.clipboard.writeText(toml);
            return true;
        } catch {
            bannerError = "Failed to copy config to clipboard";
            return false;
        }
    }

    // Parses TOML text into the draft config, surfacing a parse error in the
    // banner. The single sink the picker and the drag-drop paths both feed.
    function applyTomlText(text: string): void {
        try {
            machine.setDraft(tomlToSimParams(text));
            bannerError = null;
        } catch (err) {
            bannerError =
                err instanceof Error ? err.message : "Failed to parse config";
        }
    }

    // ── Config / snapshot loaders (shared by picker + drag-drop) ──────────────
    function loadConfigFile(file: File): Promise<void> {
        return file.text().then(applyTomlText, () => {
            bannerError = "Failed to read config file";
        });
    }

    function loadSnapshotFile(file: File): Promise<void> {
        return file.arrayBuffer().then(
            (buf) => machine.loadSnapshot(new Uint8Array(buf)),
            () => {
                bannerError = "Failed to read snapshot file";
            },
        );
    }

    async function importConfig(): Promise<void> {
        const file = await pickFile(".toml");
        if (file) await loadConfigFile(file);
    }

    async function importSnapshot(): Promise<void> {
        const file = await pickFile(".snap");
        if (file) await loadSnapshotFile(file);
    }

    // No changes are unaffordable during free-run.
    const changeDisabled = $derived(
        machine.phase === "FREE_RUNNING" ||
            machine.phase === "FREE_RUN_STOPPING",
    );

    function handleDrop(e: DragEvent): void {
        e.preventDefault();
        isDragging = false;
        if (changeDisabled) {
            bannerError = "Stop the evolution run before loading files";
            return;
        }
        const { toml, snap, error } = classifyDroppedFiles(
            Array.from(e.dataTransfer?.files ?? []),
        );
        if (error) {
            bannerError = error;
            return;
        }
        // Config and snapshot are independent: the .toml updates the draft
        // config, the .snap imports survivors into the live sim. A dropped pair
        // just runs both.
        if (toml) void loadConfigFile(toml);
        if (snap) void loadSnapshotFile(snap);
    }
</script>

<div
    class="app-shell"
    style="--topbar-h: {topbarH}px"
    ondragenter={handleDragEnter}
    ondragover={(e) => e.preventDefault()}
    role="application"
>
    {#if isDragging}
        <div
            class="drop-overlay"
            aria-hidden="true"
            ondragleave={() => (isDragging = false)}
            ondragover={(e) => e.preventDefault()}
            ondrop={handleDrop}
        >
            <div class="drop-overlay__frame" aria-hidden="true"></div>
            <div class="drop-overlay__card">
                <p class="drop-overlay__title">Drop files to load</p>
                <ul class="drop-overlay__list">
                    <li>
                        <span class="drop-overlay__ext">.toml</span>
                        simulation config
                    </li>
                    <li>
                        <span class="drop-overlay__ext">.snap</span>
                        population snapshot
                    </li>
                </ul>
                <p class="drop-overlay__limit">max 2 files at once</p>
            </div>
        </div>
    {/if}

    {#if bannerError}
        <ErrorBanner
            message={bannerError}
            onClose={() => (bannerError = null)}
        />
    {/if}

    {#if fatalError}
        <FatalErrorOverlay message={fatalError} />
    {/if}

    <!--
        Full-viewport canvas — sits at z-index 0.
        The worker renders kinetic sculpture + agents + grid interior on this surface.
        The grid is a REGION of this canvas, not a separate element.
    -->
    <!-- Hidden (not unmounted) while the brain is expanded: the OffscreenCanvas
         is transferred once, so the element must persist to keep the worker
         rendering target valid. -->
    <canvas
        bind:this={canvasEl}
        class:brain-hidden={brainExpanded}
        onclick={handleCanvasClick}
        onmousemove={handleCanvasHover}
        onmouseleave={() => focus.endHover()}
    ></canvas>

    <HoverCard agent={focus.hovered} x={mouseX} y={mouseY} />

    {#if workerReady && !brainExpanded}
        <div
            class="grid-hint"
            style="left: {gridGeom.x}px; top: {gridGeom.y - 24}px"
            aria-hidden="true"
        >
            <MousePointerClick size={13} class="icon-inline" />
            Click or Ctrl+hover an agent to inspect it
        </div>
    {/if}

    <!-- z-index: 20 — fixed top bar (contains PlayDock inline) -->
    <TopBar
        phase={machine.phase}
        genomIncompatible={machine.genomIncompatible}
        {targetSpeed}
        bind:headerHeight={topbarH}
        onSetSpeed={handleSetSpeed}
        onToggle={() => machine.toggle()}
        onStep={() => machine.step()}
        onNextGen={handleNextGen}
        onRewind={handleRewind}
        onClearGenom={() => machine.clearGenom()}
        onToggleFreeRun={() => machine.toggleFreeRun()}
        onReturnToSculpture={() => send({ type: "returnToKinematic" })}
    />

    <!-- z-index: 55/60 — config-change-while-running dialog -->
    <ConfigChangeDialog
        open={machine.phase === "CONFIRM"}
        onRevertContinue={() => machine.confirmRevertContinue()}
        onRewind={() => machine.confirmRewind()}
    />

    <!-- Grid stack — hidden while the brain explorer is expanded (it takes over
         the main area). -->
    {#if !brainExpanded}
        <!-- z-index: 5 — transparent overlay: crop marks + idle text only -->
        <GridView
            geom={gridGeom}
            phase={machine.phase}
            gridSizeX={telemetry.gridSizeX}
            gridSizeY={telemetry.gridSizeY}
            blurred={machine.phase === "CONFIRM"}
        />

        {#if machine.phase === "FREE_RUNNING" || machine.phase === "FREE_RUN_STOPPING"}
            <!-- z-index: 10 — full-grid overlay during free-run -->
            <EvolveOverlay
                geom={gridGeom}
                gen={telemetry.gen}
                phase={machine.phase}
            />
        {:else}
            <!-- z-index: 15 — telemetry stats, right of grid (below on <1160) -->
            <TelemetryHUD
                geom={gridGeom}
                placement={telemetryPlacement}
                phase={machine.phase}
                gen={telemetry.gen}
                step={telemetry.step}
                stepsPerGen={telemetry.stepsPerGen}
                pop={telemetry.pop}
                fps={measuredFps}
            />
        {/if}

        <!-- z-index: 15 — survival sparkline, bottom-left -->
        <SurvivalSparkline
            survivalHistory={telemetry.survivalHistory}
            min={telemetry.survivalMin}
            current={telemetry.survivalCurrent}
            max={telemetry.survivalMax}
            left={survivalBounds.left}
            maxWidth={survivalBounds.maxWidth}
        />
    {/if}

    <!-- Hamburger toggle — top-right, z-index: 30 -->
    <button
        class="hamburger"
        class:hamburger--open={railOpen}
        style="--hamburger-inset: {hamburgerInset(viewportW)}px;"
        onclick={() => (railOpen = !railOpen)}
        aria-label={railOpen ? "Close panel" : "Open panel"}
        aria-expanded={railOpen}
    >
        <Menu size={18} />
    </button>

    <!-- z-index: 22 — slide-in right rail with two tabs -->
    <RightRail
        open={railOpen}
        {activeTab}
        hasSelection={focus.hasSelection}
        onTabChange={(t) => (activeTab = t)}
    >
        {#snippet sim()}
            <SimConfigPanel
                draftConfig={machine.draftConfig}
                isDirty={machine.dirty}
                incompatibleFields={machine.incompatibleFields}
                requiredGenomeLen={machine.requiredGenomeLen}
                requiredNeurons={machine.requiredNeurons}
                onDraftChange={(p) => machine.setDraft(p)}
                onRevert={() => machine.revertDraft()}
                onConfUpload={() => void importConfig()}
                onConfDownload={handleConfDownload}
                onConfCopy={handleConfCopy}
                snapReady={telemetry.snapReady}
                onSnapUpload={() => void importSnapshot()}
                onSnapDownload={() => machine.exportSnapshot()}
                {changeDisabled}
            />
        {/snippet}
        {#snippet cell()}
            <CellPanel
                agent={focus.display}
                brain={displayBrain}
                isSelected={focus.isSelected}
                onClear={handleClearSelection}
                onNavigate={(dir) => focus.navigate(dir)}
                onShuffle={() => focus.shuffle()}
                onSelectById={(id) => focus.selectById(id)}
                onExpandBrain={() => (brainExpanded = true)}
            />
        {/snippet}
    </RightRail>

    <!-- Expanded brain explorer — replaces the grid stack in the main area.
         Sits below the topbar and left of the rail; no card chrome, so the
         title/knobs/synthesis float on the dotted background. -->
    {#if brainExpanded && displayBrain}
        <section
            class="brain-region"
            class:brain-region--rail={railOpen && viewportW > 760}
            aria-label="Brain explorer"
        >
            <div class="brain-region__head">
                <h2 class="brain-region__title">
                    Brain · Agent #{displayBrain.id.toString().padStart(4, "0")}
                </h2>
                <button
                    class="button button--utility brain-region__back"
                    onclick={() => (brainExpanded = false)}
                    aria-label="Back to grid"
                    title="Back to grid (Esc)"
                >
                    <ArrowLeft size={14} />
                    back to grid (esc)
                </button>
            </div>
            <BrainExplorer
                conns={displayBrain.conns}
                neuronCount={displayBrain.neuronCount}
                variant="full"
            />
        </section>
    {/if}
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

    .grid-hint {
        position: absolute;
        z-index: 6;
        pointer-events: none;
        font-family: var(--font-mono);
        font-size: var(--text-2xs);
        color: var(--color-text-muted);
    }

    .hamburger {
        position: fixed;
        right: var(--hamburger-inset, var(--space-6));
        top: calc(
            var(--topbar-h, 3.5rem) + var(--hamburger-inset, var(--space-6))
        );
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
            transform 0.26s cubic-bezier(0.2, 0.7, 0.3, 1),
            border-color 0.1s,
            color 0.1s;
    }

    .hamburger--open {
        /* Cap the rightward tuck at the available inset so it never crosses the
           viewport edge on narrow screens (where --hamburger-inset shrinks to
           8px); --space-4 preserves the desktop look. */
        transform: translateX(
                min(
                    var(--space-4),
                    calc(var(--hamburger-inset) - var(--space-1))
                )
            )
            translateY(
                max(
                    calc(-1 * var(--space-4)),
                    calc(-1 * var(--hamburger-inset) + var(--space-1))
                )
            )
            scale(0.8);
        transform-origin: top right;
    }

    .hamburger:hover {
        border-color: var(--color-accent);
        color: var(--color-accent-text);
    }

    .hamburger:focus-visible {
        outline: 2px solid var(--color-accent);
        outline-offset: 2px;
    }

    .brain-hidden {
        display: none;
    }

    /* ── Expanded brain explorer region ── */
    /* Replaces the grid stack: spans below the topbar, left of the rail. No
       card chrome — the title/knobs/synthesis float on the dotted background. */
    .brain-region {
        position: fixed;
        max-width: 1060px;
        margin-inline: auto;
        /* Offset past the build-version line that TopBar parks just under the
           bar (left-aligned, at --topbar-h + space-3); without this the title
           collides with it. Stays relative to --topbar-h so it keeps tracking
           the header's narrow-viewport reflow. */
        top: calc(var(--topbar-h, 3.5rem) + var(--space-6));
        left: 0;
        right: 0;
        bottom: 0;
        z-index: 10;
        display: flex;
        flex-direction: column;
        gap: var(--space-2);
        padding: var(--space-5) var(--space-8) var(--space-6);
    }

    .brain-region--rail {
        right: 23.75rem;
    }

    .brain-region__head {
        display: flex;
        align-items: center;
        gap: var(--space-3);
        flex-shrink: 0;
    }

    .brain-region__title {
        font-family: var(--font-sans);
        font-size: var(--text-base);
        font-weight: 700;
        color: var(--color-text);
        margin: 0;
    }

    .brain-region__back {
        margin-left: auto;
        display: inline-flex;
    }

    .brain-region__back:hover {
        color: var(--color-text);
    }

    /* BrainExplorer fills the remaining height below the floating header. */
    .brain-region :global(.brain--full) {
        height: auto;
        flex: 1;
        min-height: 0;
    }

    /* ── Drag-drop overlay ── */
    .drop-overlay {
        position: fixed;
        inset: var(--topbar-h, 3.5rem) 0 0;
        z-index: 100;
        display: flex;
        align-items: center;
        justify-content: center;
        background: color-mix(in srgb, var(--color-surface) 70%, transparent);
        backdrop-filter: blur(2px);
    }

    .drop-overlay__frame {
        position: absolute;
        inset: var(--space-4);
        border: 2px dashed var(--color-accent-border);
        border-radius: var(--radius-md);
        pointer-events: none;
    }

    .drop-overlay__card {
        display: flex;
        flex-direction: column;
        align-items: center;
        gap: var(--space-3);
        padding: var(--space-6) var(--space-8);
        background: var(--color-surface-glass);
        border: 1px solid var(--color-accent-border);
        border-radius: var(--radius-sm);
    }

    .drop-overlay__title {
        font-family: var(--font-mono);
        font-size: var(--text-base);
        font-weight: 700;
        color: var(--color-text);
        margin: 0;
        letter-spacing: var(--tracking-tight);
    }

    .drop-overlay__list {
        list-style: none;
        margin: 0;
        padding: 0;
        display: flex;
        flex-direction: column;
        gap: var(--space-2);
    }

    .drop-overlay__list li {
        font-family: var(--font-mono);
        font-size: var(--text-sm);
        color: var(--color-text);
        display: flex;
        align-items: center;
        gap: var(--space-2);
    }

    .drop-overlay__ext {
        font-weight: 700;
        color: var(--color-accent-text);
        min-width: 3.5rem;
    }

    .drop-overlay__limit {
        font-family: var(--font-mono);
        font-size: var(--text-xs);
        color: var(--color-text-muted);
        margin: 0;
    }
</style>
