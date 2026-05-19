<script lang="ts">
    let status = $state("initializing...");

    const worker = new Worker(
        new URL("./workers/sim.worker.ts", import.meta.url),
        {
            type: "module",
        },
    );
    worker.addEventListener("message", (e: MessageEvent) => {
        if (e.data === "ready") {
            console.log("[main] worker ready");
            status = "ready";
        }
    });
</script>

<h1>biosim4-gpu</h1>
<p>Status: {status}</p>
