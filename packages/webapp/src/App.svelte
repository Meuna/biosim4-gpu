<script lang="ts">
    import type {
        WorkerCmd,
        WorkerEvent,
        AgentInfo,
        SimParams,
    } from "./workers/sim.worker";
    import TopBar from "./lib/TopBar.svelte";
    import GridView from "./lib/GridView.svelte";
    import HUD from "./lib/HUD.svelte";
    import TelemetryHUD from "./lib/TelemetryHUD.svelte";
    import RightRail from "./lib/RightRail.svelte";
    import SimConfigPanel from "./lib/SimConfigPanel.svelte";
    import CellPanel from "./lib/CellPanel.svelte";
    import HoverCard from "./lib/HoverCard.svelte";
    import BrainExplorer from "./lib/BrainExplorer.svelte";
    import ConfigChangeDialog from "./lib/ConfigChangeDialog.svelte";
    import EvolveOverlay from "./lib/EvolveOverlay.svelte";
    import type { BrainConn } from "./lib/brain";
    import { MousePointerClick, ArrowLeft, Menu } from "lucide-svelte";
    import {
        DEFAULTS,
        simParamsToToml,
        tomlToSimParams,
    } from "./lib/tomlConfig";

    // ── Canvas / worker ──────────────────────────────────────────────────────
    let canvasEl = $state<HTMLCanvasElement | undefined>();
    let workerReady = $state(false);

    const worker = new Worker(
        new URL("./workers/sim.worker.ts", import.meta.url),
        { type: "module" },
    );

    worker.addEventListener("message", (e: MessageEvent<WorkerEvent>) => {
        const msg = e.data;
        if (msg.type === "agentPicked") {
            if (msg.reason === "click") {
                selectedAgent = msg.info;
                lastHoveredAgent = null;
                send({ type: "selectAgent", id: msg.info.id });
            } else {
                hoveredAgent = msg.info;
                lastHoveredAgent = msg.info;
                send({ type: "hoverAgent", id: msg.info.id });
            }
        } else if (msg.type === "agentMissed") {
            if (msg.reason === "click") {
                selectedAgent = null;
                send({ type: "selectAgent", id: null });
            } else {
                hoveredAgent = null;
                send({ type: "hoverAgent", id: null });
            }
        } else if (msg.type === "ready") {
            if (canvasEl) {
                const offscreen = canvasEl.transferControlToOffscreen();
                // Read raw tokens directly (not via var() aliases) so
                // getPropertyValue resolves to usable color strings.
                const styles = getComputedStyle(document.documentElement);
                const overlayColor = styles
                    .getPropertyValue("--_border-subtle")
                    .trim();
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
                workerReady = true;
                // Send initial layout so the worker sizes the canvas and knows
                // where the grid region lives on the full-viewport surface.
                send({
                    type: "layout",
                    canvasW: viewportW,
                    canvasH: viewportH,
                    gridX: gridGeom.x,
                    gridY: gridGeom.y,
                    gridW: gridGeom.w,
                    gridH: gridGeom.h,
                    gridCellsX: gridSizeX,
                    gridCellsY: gridSizeY,
                } satisfies WorkerCmd);
            }
        } else if (msg.type === "status") {
            currentStep = msg.step;
            if (msg.state === "gen_complete") {
                isRunning = false;
                isGenComplete = true;
            } else if (msg.state === "paused" && isFreeRunStopping) {
                isFreeRunning = false;
                isFreeRunStopping = false;
            }
        } else if (msg.type === "census") {
            currentGen = msg.gen;
            currentPop = msg.population;
            const rate =
                msg.population > 0 ? msg.survivors / msg.population : 0;
            survivalHistory = [...survivalHistory.slice(-11), rate];
            workerGenomeMaxLenUsed = msg.genomeMaxLenUsed;
            workerGenomeMaxNeuronsUsed = msg.genomeMaxNeuronsUsed;
            snapReady = true;
        } else if (msg.type === "rewindConfigured") {
            isGenComplete = false;
            currentGen = msg.gen;
            currentStep = 0;
            currentPop = msg.population;
            gridSizeX = msg.gridSizeX;
            gridSizeY = msg.gridSizeY;
            stepsPerGen = msg.stepsPerGen;
            survivalHistory = [];
            workerGenomeMaxLenUsed = 0;
            workerGenomeMaxNeuronsUsed = 0;
            if (pendingLastPlayedConfig !== null) {
                lastPlayedConfig = pendingLastPlayedConfig;
                pendingLastPlayedConfig = null;
            }
        } else if (msg.type === "nextGenerationConfigured") {
            isGenComplete = false;
            currentGen = msg.gen;
            currentStep = 0;
            currentPop = msg.population;
            gridSizeX = msg.gridSizeX;
            gridSizeY = msg.gridSizeY;
            stepsPerGen = msg.stepsPerGen;
            const rate =
                msg.censusPopulation > 0
                    ? msg.survivors / msg.censusPopulation
                    : 0;
            survivalHistory = [...survivalHistory.slice(-11), rate];
            workerGenomeMaxLenUsed = msg.genomeMaxLenUsed;
            workerGenomeMaxNeuronsUsed = msg.genomeMaxNeuronsUsed;
            snapReady = true;
            if (pendingLastPlayedConfig !== null) {
                lastPlayedConfig = pendingLastPlayedConfig;
                pendingLastPlayedConfig = null;
            }
        } else if (msg.type === "agentUpdated") {
            selectedAgent = msg.info;
        } else if (msg.type === "brainData") {
            // Ignore brain payloads for an agent that is no longer displayed.
            if (msg.id === displayAgent?.id) {
                brain = {
                    id: msg.id,
                    conns: msg.conns,
                    neuronCount: msg.neuronCount,
                };
            }
        } else if (msg.type === "fps") {
            measuredFps = msg.value;
        } else if (msg.type === "configured") {
            isRunning = false;
            hasStarted = false;
            isGenComplete = false;
            currentGen = 0;
            currentStep = 0;
            survivalHistory = [];
            currentPop = msg.population;
            gridSizeX = msg.gridSizeX;
            gridSizeY = msg.gridSizeY;
            stepsPerGen = msg.stepsPerGen;
            workerGenomeMaxLenUsed = 0;
            workerGenomeMaxNeuronsUsed = 0;
            snapReady = false;
            if (pendingLastPlayedConfig !== null) {
                lastPlayedConfig = pendingLastPlayedConfig;
                pendingLastPlayedConfig = null;
            }
            if (pendingPlay) {
                pendingPlay = false;
                isRunning = true;
                hasStarted = true;
                send({ type: "play" });
            }
        } else if (msg.type === "snapshotData") {
            const blob = new Blob([msg.data], {
                type: "application/octet-stream",
            });
            const url = URL.createObjectURL(blob);
            const a = document.createElement("a");
            a.href = url;
            a.download = "biosim.snap";
            a.click();
            URL.revokeObjectURL(url);
        } else if (msg.type === "snapshotLoaded") {
            isGenComplete = false;
            currentGen = msg.gen;
            currentStep = 0;
            currentPop = msg.population;
            gridSizeX = msg.gridSizeX;
            gridSizeY = msg.gridSizeY;
            stepsPerGen = msg.stepsPerGen;
            hasStarted = true;
            snapReady = true;
            survivalHistory = [];
            workerGenomeMaxLenUsed = 0;
            workerGenomeMaxNeuronsUsed = 0;
            if (pendingLastPlayedConfig !== null) {
                lastPlayedConfig = pendingLastPlayedConfig;
                pendingLastPlayedConfig = null;
            }
        } else if (msg.type === "error") {
            confErrorMsg = msg.message;
        }
    });

    function send(cmd: WorkerCmd): void {
        worker.postMessage(cmd);
    }

    // ── Draft / last-played config ───────────────────────────────────────────
    let draftConfig = $state<SimParams>({
        ...DEFAULTS,
        challenge: { ...DEFAULTS.challenge },
    });
    let lastPlayedConfig = $state<SimParams>({
        ...DEFAULTS,
        challenge: { ...DEFAULTS.challenge },
    });
    const isDirty = $derived(
        JSON.stringify($state.snapshot(draftConfig)) !==
            JSON.stringify($state.snapshot(lastPlayedConfig)),
    );
    // Plain (non-reactive) — only read inside the worker message handler.
    let pendingPlay = false;
    let pendingLastPlayedConfig: SimParams | null = null;

    // ── Simulation state ─────────────────────────────────────────────────────
    let isRunning = $state(false);
    let hasStarted = $state(false);
    let isGenComplete = $state(false);
    let isFreeRunning = $state(false);
    let isFreeRunStopping = $state(false);
    const mode = $derived(
        isRunning ? "running" : hasStarted ? "paused" : "kinetic",
    ) as "kinetic" | "running" | "paused";
    let currentGen = $state(0);
    let currentStep = $state(0);
    let currentPop = $state(3000);
    let survivalHistory = $state<number[]>([]);

    // These reflect the active simulation parameters and are updated when the
    // config panel applies a new configuration.
    let stepsPerGen = $state(300);
    let gridSizeX = $state(128);
    let gridSizeY = $state(128);

    // Genome max values from the last census; reset to 0 after configure/clearGenom.
    let workerGenomeMaxLenUsed = $state(0);
    let workerGenomeMaxNeuronsUsed = $state(0);

    // True once at least one generation boundary has been crossed (snap.count > 0).
    let snapReady = $state(false);
    // Bound to the hidden .snap file input element.
    let snapUploadInput = $state<HTMLInputElement | null>(null);

    // ── Speed / FPS ──────────────────────────────────────────────────────────
    let targetSpeed = $state(0);
    let measuredFps = $state<number | null>(null);

    $effect(() => {
        if (!isRunning) measuredFps = null;
    });

    // Genome compatibility gate — true when draft would truncate live survivors.
    const genomIncompatible = $derived(
        (workerGenomeMaxLenUsed > 0 &&
            draftConfig.maxGenomeLen < workerGenomeMaxLenUsed) ||
            (workerGenomeMaxNeuronsUsed > 0 &&
                draftConfig.maxNeurons < workerGenomeMaxNeuronsUsed),
    );
    const incompatibleFields = $derived<string[]>([
        ...(workerGenomeMaxLenUsed > 0 &&
        draftConfig.maxGenomeLen < workerGenomeMaxLenUsed
            ? ["maxGenomeLen"]
            : []),
        ...(workerGenomeMaxNeuronsUsed > 0 &&
        draftConfig.maxNeurons < workerGenomeMaxNeuronsUsed
            ? ["maxNeurons"]
            : []),
    ]);

    // ── UI state ─────────────────────────────────────────────────────────────
    let selectedAgent = $state<AgentInfo | null>(null);
    let hoveredAgent = $state<AgentInfo | null>(null);
    let lastHoveredAgent = $state<AgentInfo | null>(null);
    let mouseX = $state(0);
    let mouseY = $state(0);
    const displayAgent = $derived(
        hoveredAgent ?? lastHoveredAgent ?? selectedAgent,
    );
    let railOpen = $state(false);
    let activeTab = $state<"sim" | "cell">("sim");
    let showConfigChangeDialog = $state(false);
    let gridBlurred = $state(false);

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
        const id = displayAgent?.id ?? null;
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
        brain && brain.id === displayAgent?.id ? brain : null,
    );

    // Open Cell tab automatically when an agent is selected or hovered.
    $effect(() => {
        if (displayAgent !== null) {
            railOpen = true;
            activeTab = "cell";
        }
    });

    // Clear hover state when Ctrl key is released.
    $effect(() => {
        function onKeyUp(e: KeyboardEvent): void {
            if (e.key === "Control") {
                hoveredAgent = null;
                lastHoveredAgent = null;
                send({ type: "hoverAgent", id: null });
            }
        }
        window.addEventListener("keyup", onKeyUp);
        return () => window.removeEventListener("keyup", onKeyUp);
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
        const maxCells = Math.max(gridSizeX, gridSizeY);
        const maxDim = Math.max(140, Math.min(availW, availH, 760));
        const ppc = maxDim / maxCells;
        const w = gridSizeX * ppc;
        const h = gridSizeY * ppc;
        const x = PAD_SIDE + (availW - w) / 2;
        const y = TOPBAR_H + PAD_TOP + (availH - h) / 2;
        return { x, y, w, h, cx: x + w / 2, cy: y + h / 2 };
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
            gridW: gridGeom.w,
            gridH: gridGeom.h,
            gridCellsX: gridSizeX,
            gridCellsY: gridSizeY,
        } satisfies WorkerCmd);
    });

    // ── Draft config callbacks ────────────────────────────────────────────────
    function handleDraftChange(params: SimParams): void {
        draftConfig = params;
        if (
            !isGenComplete &&
            (isRunning || hasStarted) &&
            !showConfigChangeDialog
        ) {
            if (isRunning) {
                isRunning = false;
                send({ type: "stop" });
            }
            if (isFreeRunning) {
                isFreeRunStopping = true;
                send({ type: "stopFreeRun" });
            }
            gridBlurred = true;
            showConfigChangeDialog = true;
        }
    }

    function handleRevert(): void {
        draftConfig = { ...($state.snapshot(lastPlayedConfig) as SimParams) };
    }

    function handleDialogRevertContinue(): void {
        handleRevert();
        showConfigChangeDialog = false;
        gridBlurred = false;
        isRunning = true;
        hasStarted = true;
        send({ type: "play" });
    }

    function handleDialogRewind(): void {
        showConfigChangeDialog = false;
        gridBlurred = false;
        handleRewind(false);
    }

    // ── Play / pause / step / gen / reset ────────────────────────────────────
    function handleToggle(): void {
        if (isRunning) {
            isRunning = false;
            send({ type: "stop" });
        } else if (genomIncompatible) {
            return;
        } else if (isDirty) {
            pendingPlay = true;
            isGenComplete = false;
            const snapshot = $state.snapshot(draftConfig) as SimParams;
            pendingLastPlayedConfig = snapshot;
            send({ type: "configure", params: snapshot });
        } else {
            isRunning = true;
            hasStarted = true;
            isGenComplete = false;
            send({ type: "play" });
        }
    }

    function handleSetSpeed(fps: number): void {
        targetSpeed = fps;
        send({ type: "setSpeed", fps } satisfies WorkerCmd);
    }

    function handleStep(): void {
        hasStarted = true;
        send({ type: "step" });
    }

    function handleToggleFreeRun(): void {
        if (isFreeRunning) {
            isFreeRunStopping = true;
            send({ type: "stopFreeRun" });
        } else {
            isFreeRunning = true;
            hasStarted = true;
            isGenComplete = false;
            send({ type: "startFreeRun" });
        }
    }

    function handleNextGen(autoPlay: boolean): void {
        isGenComplete = false;
        hasStarted = true;
        if (isDirty) {
            const snapshot = $state.snapshot(draftConfig) as SimParams;
            pendingLastPlayedConfig = snapshot;
            send({ type: "nextGenerationConfigured", params: snapshot });
        } else {
            send({ type: "nextGeneration" });
        }
        if (autoPlay) {
            isRunning = true;
            send({ type: "play" });
        }
    }

    function handleRewind(autoPlay: boolean): void {
        isGenComplete = false;
        if (isDirty) {
            const snapshot = $state.snapshot(draftConfig) as SimParams;
            pendingLastPlayedConfig = snapshot;
            send({ type: "rewindConfigured", params: snapshot });
        } else {
            send({ type: "rewind" });
        }
        if (autoPlay) {
            isRunning = true;
            hasStarted = true;
            send({ type: "play" });
        }
    }

    function handleClearGenom(): void {
        isGenComplete = false;
        workerGenomeMaxLenUsed = 0;
        workerGenomeMaxNeuronsUsed = 0;
        send({ type: "clearGenom" });
    }

    // ── Agent selection ───────────────────────────────────────────────────────
    function handleClearSelection(): void {
        selectedAgent = null;
        hoveredAgent = null;
        lastHoveredAgent = null;
        send({ type: "selectAgent", id: null });
        send({ type: "hoverAgent", id: null });
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
            gx: Math.floor((px - gridGeom.x) / (gridGeom.w / gridSizeX)),
            gy: Math.floor((py - gridGeom.y) / (gridGeom.h / gridSizeY)),
        };
    }

    function handleCanvasClick(e: MouseEvent): void {
        if (!workerReady) return;
        if (e.ctrlKey && lastHoveredAgent !== null && hoveredAgent === null) {
            selectedAgent = lastHoveredAgent;
            lastHoveredAgent = null;
            send({ type: "selectAgent", id: selectedAgent.id });
            return;
        }
        const cell = pixelToCell(e.clientX, e.clientY);
        if (cell) send({ type: "pickAgentAtCell", ...cell, reason: "click" });
    }

    let hoverRafPending = false;
    function handleCanvasHover(e: MouseEvent): void {
        mouseX = e.clientX;
        mouseY = e.clientY;
        if (!e.ctrlKey || !workerReady) {
            if (hoveredAgent !== null) {
                hoveredAgent = null;
                send({ type: "hoverAgent", id: null });
            }
            return;
        }
        if (hoverRafPending) return;
        hoverRafPending = true;
        const px = e.clientX;
        const py = e.clientY;
        requestAnimationFrame(() => {
            hoverRafPending = false;
            const cell = pixelToCell(px, py);
            if (cell)
                send({ type: "pickAgentAtCell", ...cell, reason: "hover" });
        });
    }

    function handleNavigate(dir: -1 | 1): void {
        if (selectedAgent) {
            send({
                type: "navigateAgent",
                fromId: selectedAgent.id,
                direction: dir,
            });
        }
    }

    function handleShuffle(): void {
        send({ type: "randomAgent" });
    }

    function handleSelectById(id: number): void {
        send({ type: "selectAgentById", id });
    }

    // ── Drag-drop overlay ────────────────────────────────────────────────────
    let isDragging = $state(false);

    function handleDragEnter(e: DragEvent): void {
        if (!e.dataTransfer?.types.includes("Files")) return;
        isDragging = true;
    }

    // ── Config import / export ───────────────────────────────────────────────
    let uploadInput = $state<HTMLInputElement | null>(null);
    let confErrorMsg = $state<string | null>(null);

    $effect(() => {
        if (!confErrorMsg) return;
        const t = setTimeout(() => {
            confErrorMsg = null;
        }, 4000);
        return () => clearTimeout(t);
    });

    function handleConfDownload(): void {
        const toml = simParamsToToml($state.snapshot(draftConfig) as SimParams);
        const blob = new Blob([toml], { type: "text/plain" });
        const url = URL.createObjectURL(blob);
        const a = document.createElement("a");
        a.href = url;
        a.download = "biosim.toml";
        a.click();
        URL.revokeObjectURL(url);
    }

    function handleConfUpload(): void {
        uploadInput?.click();
    }

    function applyTomlText(text: string): void {
        try {
            handleDraftChange(tomlToSimParams(text));
            confErrorMsg = null;
        } catch (err) {
            confErrorMsg =
                err instanceof Error ? err.message : "Failed to parse config";
        }
    }

    function handleFileInputChange(
        e: Event & { currentTarget: HTMLInputElement },
    ): void {
        const file = e.currentTarget.files?.[0];
        if (!file) return;
        e.currentTarget.value = "";
        void file.text().then(
            (text) => applyTomlText(text),
            () => {
                confErrorMsg = "Failed to read config file";
            },
        );
    }

    function handleSnapUpload(): void {
        snapUploadInput?.click();
    }

    function handleSnapFileInputChange(
        e: Event & { currentTarget: HTMLInputElement },
    ): void {
        const file = e.currentTarget.files?.[0];
        if (!file) return;
        e.currentTarget.value = "";
        void file.arrayBuffer().then(
            (buf) => {
                worker.postMessage(
                    {
                        type: "loadSnapshot",
                        params: $state.snapshot(draftConfig) as SimParams,
                        data: new Uint8Array(buf),
                    } satisfies WorkerCmd,
                    [buf],
                );
            },
            () => {
                confErrorMsg = "Failed to read snapshot file";
            },
        );
    }

    function handleSnapDownload(): void {
        worker.postMessage({ type: "exportSnapshot" } satisfies WorkerCmd);
    }

    function handleDrop(e: DragEvent): void {
        e.preventDefault();
        isDragging = false;
        const files = Array.from(e.dataTransfer?.files ?? []);
        if (files.length > 2) {
            confErrorMsg = "Drop at most 2 files (one .toml, one snapshot)";
            return;
        }
        const tomlFile = files.find((f) => f.name.endsWith(".toml")) ?? null;
        const snapFile = files.find((f) => !f.name.endsWith(".toml")) ?? null;

        if (tomlFile && snapFile) {
            // Read both in parallel; parse TOML first so loadSnapshot receives
            // the correct params rather than stale draftConfig.
            void Promise.all([tomlFile.text(), snapFile.arrayBuffer()]).then(
                ([tomlText, snapBuf]) => {
                    let params: SimParams;
                    try {
                        params = tomlToSimParams(tomlText);
                        handleDraftChange(params);
                        confErrorMsg = null;
                    } catch (err) {
                        confErrorMsg =
                            err instanceof Error
                                ? err.message
                                : "Failed to parse config";
                        return;
                    }
                    worker.postMessage(
                        {
                            type: "loadSnapshot",
                            params,
                            data: new Uint8Array(snapBuf),
                        } satisfies WorkerCmd,
                        [snapBuf],
                    );
                },
                () => {
                    confErrorMsg = "Failed to read files";
                },
            );
        } else {
            if (tomlFile) {
                void tomlFile.text().then(
                    (text) => applyTomlText(text),
                    () => {
                        confErrorMsg = "Failed to read config file";
                    },
                );
            }
            if (snapFile) {
                void snapFile.arrayBuffer().then(
                    (buf) => {
                        worker.postMessage(
                            {
                                type: "loadSnapshot",
                                params: $state.snapshot(
                                    draftConfig,
                                ) as SimParams,
                                data: new Uint8Array(buf),
                            } satisfies WorkerCmd,
                            [buf],
                        );
                    },
                    () => {
                        confErrorMsg = "Failed to read snapshot file";
                    },
                );
            }
        }
    }
</script>

<div
    class="app-shell"
    ondragenter={handleDragEnter}
    ondragover={(e) => e.preventDefault()}
    role="application"
>
    <input
        bind:this={uploadInput}
        type="file"
        accept=".toml"
        style="display:none"
        onchange={handleFileInputChange}
    />
    <input
        bind:this={snapUploadInput}
        type="file"
        accept=".snap"
        style="display:none"
        onchange={handleSnapFileInputChange}
    />

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

    {#if confErrorMsg}
        <div class="conf-error-banner" role="alert">{confErrorMsg}</div>
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
        onmouseleave={() => {
            if (hoveredAgent !== null) send({ type: "hoverAgent", id: null });
            hoveredAgent = null;
        }}
    ></canvas>

    <HoverCard agent={hoveredAgent} x={mouseX} y={mouseY} />

    {#if workerReady && !brainExpanded}
        <div
            class="grid-hint"
            style="left: {gridGeom.x}px; top: {gridGeom.y - 24}px"
            aria-hidden="true"
        >
            <MousePointerClick size={13} style="display: inline-block; " />
            Click or Ctrl+hover an agent to inspect it
        </div>
    {/if}

    <!-- z-index: 20 — fixed top bar (contains PlayDock inline) -->
    <TopBar
        running={isRunning}
        genComplete={isGenComplete}
        {genomIncompatible}
        freeRunning={isFreeRunning}
        freeRunStopping={isFreeRunStopping}
        {targetSpeed}
        onSetSpeed={handleSetSpeed}
        onToggle={handleToggle}
        onStep={handleStep}
        onNextGen={handleNextGen}
        onRewind={handleRewind}
        onClearGenom={handleClearGenom}
        onToggleFreeRun={handleToggleFreeRun}
    />

    <!-- z-index: 55/60 — config-change-while-running dialog -->
    <ConfigChangeDialog
        open={showConfigChangeDialog}
        onRevertContinue={handleDialogRevertContinue}
        onRewind={handleDialogRewind}
    />

    <!-- Grid stack — hidden while the brain explorer is expanded (it takes over
         the main area). -->
    {#if !brainExpanded}
        <!-- z-index: 5 — transparent overlay: crop marks + idle text only -->
        <GridView
            geom={gridGeom}
            {mode}
            {gridSizeX}
            {gridSizeY}
            blurred={gridBlurred}
        />

        {#if isFreeRunning}
            <!-- z-index: 10 — full-grid overlay during free-run -->
            <EvolveOverlay
                geom={gridGeom}
                gen={currentGen}
                stopping={isFreeRunStopping}
            />
        {:else}
            <!-- z-index: 15 — telemetry stats, top-right of grid -->
            <TelemetryHUD
                geom={gridGeom}
                running={isRunning}
                gen={currentGen}
                step={currentStep}
                {stepsPerGen}
                pop={currentPop}
                fps={measuredFps}
            />

            <!-- z-index: 15 — survival sparkline, bottom-left -->
            <HUD {survivalHistory} />
        {/if}
    {/if}

    <!-- Hamburger toggle — top-right, z-index: 30 -->
    <button
        class="hamburger"
        class:hamburger--open={railOpen}
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
        hasSelection={displayAgent !== null}
        onTabChange={(t) => (activeTab = t)}
    >
        {#snippet sim()}
            <SimConfigPanel
                {draftConfig}
                {isDirty}
                {incompatibleFields}
                genomeMaxLenUsed={workerGenomeMaxLenUsed}
                genomeMaxNeuronsUsed={workerGenomeMaxNeuronsUsed}
                onDraftChange={handleDraftChange}
                onRevert={handleRevert}
                onConfUpload={handleConfUpload}
                onConfDownload={handleConfDownload}
                {snapReady}
                onSnapUpload={handleSnapUpload}
                onSnapDownload={handleSnapDownload}
            />
        {/snippet}
        {#snippet cell()}
            <CellPanel
                agent={displayAgent}
                brain={displayBrain}
                isSelected={selectedAgent !== null &&
                    hoveredAgent === null &&
                    lastHoveredAgent === null}
                onClear={handleClearSelection}
                onNavigate={handleNavigate}
                onShuffle={handleShuffle}
                onSelectById={handleSelectById}
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
        font-size: 10px;
        color: var(--color-text-muted);
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
            transform 0.26s cubic-bezier(0.2, 0.7, 0.3, 1),
            border-color 0.1s,
            color 0.1s;
    }

    .hamburger--open {
        transform: translateX(var(--space-4))
            translateY(calc(-1 * var(--space-3))) scale(0.6);
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
        top: 3.5rem;
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
        inset: 3.5rem 0 0;
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
        letter-spacing: -0.01em;
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
        letter-spacing: 0.05em;
    }

    .conf-error-banner {
        position: fixed;
        top: var(--space-4);
        right: var(--space-4);
        z-index: 90;
        max-width: 26rem;
        padding: var(--space-2) var(--space-4);
        background: var(--color-surface-glass);
        border: 1px solid var(--color-warn);
        color: var(--color-warn);
        font-family: var(--font-mono);
        font-size: var(--text-sm);
        line-height: 1.4;
        word-break: break-word;
        backdrop-filter: blur(4px);
    }
</style>
