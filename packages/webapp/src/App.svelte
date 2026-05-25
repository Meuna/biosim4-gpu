<script lang="ts">
    import type { WorkerCmd, WorkerEvent } from "./workers/sim.worker";

    let status = $state("initializing...");
    let events = $state<string[]>([]);
    let canvasEl = $state<HTMLCanvasElement | undefined>();

    const worker = new Worker(
        new URL("./workers/sim.worker.ts", import.meta.url),
        { type: "module" },
    );

    worker.addEventListener("message", (e: MessageEvent<WorkerEvent>) => {
        const msg = e.data;
        if (msg.type === "ready") {
            status = "ready";
            if (canvasEl) {
                const offscreen = canvasEl.transferControlToOffscreen();
                worker.postMessage(
                    { type: "canvas", canvas: offscreen } satisfies WorkerCmd,
                    [offscreen],
                );
            }
        } else if (msg.type === "status") {
            status = msg.message;
        } else if (msg.type === "census") {
            events = [
                `Gen ${msg.gen}: ${msg.survivors}/${msg.population} survivors, ${msg.kills} kills`,
                ...events,
            ];
        } else if (msg.type === "error") {
            status = `error: ${msg.message}`;
        }
    });

    function send(cmd: WorkerCmd): void {
        worker.postMessage(cmd);
    }
</script>

<header class="app-header">
    <span class="header-index">01</span>
    <h1 class="app-title">biosim4-gpu</h1>
</header>

<main class="app-main">
    <section class="controls" aria-label="Simulation controls">
        <div class="btn-group">
            <button onclick={() => send({ type: "play" })}>Play</button>
            <button onclick={() => send({ type: "stop" })}>Stop</button>
            <button onclick={() => send({ type: "step" })}>Step</button>
            <button onclick={() => send({ type: "stepAgent" })}
                >Step Agent</button
            >
            <button onclick={() => send({ type: "nextGeneration" })}
                >New Generation</button
            >
        </div>
    </section>

    <div class="status-bar">
        <span class="status-label">STATUS</span>
        <span class="status-value">{status}</span>
    </div>

    <section class="viewport" aria-label="Simulation viewport">
        <canvas bind:this={canvasEl}></canvas>
    </section>

    <section class="event-log" aria-label="Event log">
        <div class="event-log-header">
            <span class="section-index">02</span>
            <span class="section-title">EVENT LOG</span>
        </div>
        {#each events as event}
            <p class="event">{event}</p>
        {/each}
    </section>
</main>

<style>
    .app-header {
        display: flex;
        align-items: baseline;
        gap: var(--space-4);
        padding: var(--space-8);
        padding-bottom: var(--space-6);
        border-bottom: 1px solid var(--color-border);
    }

    .header-index {
        font-family: var(--font-mono);
        font-size: var(--text-sm);
        color: var(--color-text-muted);
        letter-spacing: 0.1em;
    }

    .app-title {
        font-size: var(--text-4xl);
        font-weight: 800;
        letter-spacing: -0.03em;
        line-height: 1;
        color: var(--color-text);
    }

    .app-main {
        display: flex;
        flex-direction: column;
    }

    .controls {
        padding: var(--space-4) var(--space-8);
        border-bottom: 1px solid var(--color-border);
    }

    .btn-group {
        display: flex;
        gap: var(--space-2);
        flex-wrap: wrap;
    }

    .btn-group button {
        font-family: var(--font-mono);
        font-size: var(--text-sm);
        color: var(--color-text);
        background: transparent;
        border: 1px solid var(--color-border);
        border-radius: var(--radius-sm);
        padding: var(--space-1) var(--space-3);
        cursor: pointer;
        text-transform: uppercase;
        letter-spacing: 0.08em;
        transition:
            border-color 0.1s,
            color 0.1s;
    }

    .btn-group button:hover {
        border-color: var(--color-accent);
        color: var(--color-accent);
    }

    .status-bar {
        display: flex;
        align-items: center;
        gap: var(--space-4);
        padding: var(--space-2) var(--space-8);
        border-bottom: 1px solid var(--color-border);
        font-family: var(--font-mono);
        font-size: var(--text-sm);
    }

    .status-label {
        color: var(--color-text-muted);
        letter-spacing: 0.1em;
        font-size: 0.75rem;
    }

    .status-value {
        color: var(--color-text);
    }

    .viewport {
        padding: var(--space-4) var(--space-8);
    }

    .viewport canvas {
        display: block;
        border: 1px solid var(--color-border);
        border-radius: var(--radius-sm);
    }

    .event-log {
        padding: var(--space-4) var(--space-8);
        border-top: 1px solid var(--color-border);
    }

    .event-log-header {
        display: flex;
        align-items: center;
        gap: var(--space-3);
        margin-bottom: var(--space-3);
    }

    .section-index {
        font-family: var(--font-mono);
        font-size: var(--text-sm);
        color: var(--color-text-muted);
        letter-spacing: 0.1em;
    }

    .section-title {
        font-family: var(--font-mono);
        font-size: var(--text-sm);
        color: var(--color-text-muted);
        letter-spacing: 0.1em;
        text-transform: uppercase;
    }

    .event {
        font-family: var(--font-mono);
        font-size: var(--text-sm);
        color: var(--color-text-muted);
        padding: var(--space-1) 0;
        border-bottom: 1px solid var(--color-border);
    }

    .event:first-of-type {
        color: var(--color-text);
    }
</style>
