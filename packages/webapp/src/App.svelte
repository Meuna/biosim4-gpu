<script lang="ts">
    import type { WorkerCmd, WorkerEvent } from "./workers/sim.worker";

    let status = $state("initializing...");
    let events = $state<string[]>([]);

    const worker = new Worker(
        new URL("./workers/sim.worker.ts", import.meta.url),
        { type: "module" },
    );

    worker.addEventListener("message", (e: MessageEvent<WorkerEvent>) => {
        const msg = e.data;
        if (msg.type === "ready") {
            status = "ready";
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

<h1>biosim4-gpu</h1>

<div>
    <button onclick={() => send({ type: "play" })}>Play</button>
    <button onclick={() => send({ type: "stop" })}>Stop</button>
    <button onclick={() => send({ type: "step" })}>Step</button>
    <button onclick={() => send({ type: "stepAgent" })}>Step Agent</button>
    <button onclick={() => send({ type: "nextGeneration" })}
        >New Generation</button
    >
</div>

<div>
    <p>Status: {status}</p>
</div>

<div>
    {#each events as event}
        <p>{event}</p>
    {/each}
</div>
