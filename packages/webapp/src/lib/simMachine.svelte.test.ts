import { describe, it, expect } from "vitest";
import type { SimParams, WorkerCmd } from "../workers/sim.worker";
import { SIM_PHASES, SimMachine } from "./simMachine.svelte";
import { DEFAULTS } from "./tomlConfig";

function makeParams(overrides: Partial<SimParams> = {}): SimParams {
    return { ...structuredClone(DEFAULTS), ...overrides };
}

function create(initial?: SimParams) {
    const sent: WorkerCmd[] = [];
    const transfers: (Transferable[] | undefined)[] = [];
    const m = new SimMachine((cmd, transfer) => {
        sent.push(cmd);
        transfers.push(transfer);
    }, initial);
    return { m, sent, transfers };
}

function types(sent: WorkerCmd[]): string[] {
    return sent.map((c) => c.type);
}

describe("SIM_PHASES", () => {
    it("contains 9 phases", () => {
        expect(SIM_PHASES.length).toBe(9);
    });
});

describe("initial state", () => {
    it("starts in WORKER_PENDING, clean, with nothing sent", () => {
        const { m, sent } = create();
        expect(m.phase).toBe("WORKER_PENDING");
        expect(m.dirty).toBe(false);
        expect(m.genomIncompatible).toBe(false);
        expect(m.incompatibleFields).toEqual([]);
        expect(sent).toEqual([]);
    });

    it("clones the initial config so later mutation cannot alias it", () => {
        const initial = makeParams();
        const { m } = create(initial);
        initial.population += 1;
        expect(m.dirty).toBe(false);
        expect(m.draftConfig.population).toBe(DEFAULTS.population);
    });
});

describe("onWorkerReady", () => {
    it("transitions WORKER_PENDING to WORKER_READY", () => {
        const { m } = create();
        m.onWorkerReady();
        expect(m.phase).toBe("WORKER_READY");
    });

    it("leaves any other phase unchanged", () => {
        const { m } = create();
        m.onWorkerReady();
        m.toggle();
        m.onWorkerReady();
        expect(m.phase).toBe("STEPS_RUNNING");
    });
});

describe("toggle", () => {
    it("plays from WORKER_READY when clean", () => {
        const { m, sent } = create();
        m.onWorkerReady();
        m.toggle();
        expect(m.phase).toBe("STEPS_RUNNING");
        expect(types(sent)).toEqual(["play"]);
    });

    it("plays from GENERATION_SPAWNED and STEPS_PAUSED when clean", () => {
        for (const setup of [
            (m: SimMachine) => m.onConfigured(), // -> GENERATION_SPAWNED
            (m: SimMachine) => m.step(), // WORKER_READY -> STEPS_PAUSED
        ]) {
            const { m, sent } = create();
            m.onWorkerReady();
            setup(m);
            sent.length = 0;
            m.toggle();
            expect(m.phase).toBe("STEPS_RUNNING");
            expect(types(sent)).toEqual(["play"]);
        }
    });

    it("stops a clean run to STEPS_PAUSED", () => {
        const { m, sent } = create();
        m.onWorkerReady();
        m.toggle();
        m.toggle();
        expect(m.phase).toBe("STEPS_PAUSED");
        expect(types(sent)).toEqual(["play", "stop"]);
    });

    it("stops a dirty run into CONFIRM", () => {
        const { m, sent } = create();
        m.onWorkerReady();
        m.toggle();
        m.setDraft(makeParams({ population: DEFAULTS.population + 1 }));
        m.toggle();
        expect(m.phase).toBe("CONFIRM");
        expect(m.dirty).toBe(true);
        expect(types(sent)).toEqual(["play", "stop"]);
    });

    it("configures first when dirty, then plays on the configured reply", () => {
        const { m, sent } = create();
        m.onWorkerReady();
        const params = makeParams({ population: DEFAULTS.population + 1 });
        m.setDraft(params);
        m.toggle();
        // Phase unchanged until the worker confirms the new config.
        expect(m.phase).toBe("WORKER_READY");
        expect(sent).toEqual([{ type: "configure", params }]);
        m.onConfigured();
        expect(m.phase).toBe("STEPS_RUNNING");
        expect(types(sent)).toEqual(["configure", "play"]);
        expect(m.dirty).toBe(false);
    });

    it("is gated by genomIncompatible for play but not for stop", () => {
        const { m, sent } = create(makeParams({ maxGenomeLen: 24 }));
        m.onWorkerReady();
        m.toggle();
        m.onCensus(32, 0);
        expect(m.genomIncompatible).toBe(true);
        m.toggle(); // stop still allowed
        expect(m.phase).toBe("STEPS_PAUSED");
        m.toggle(); // play refused
        expect(m.phase).toBe("STEPS_PAUSED");
        expect(types(sent)).toEqual(["play", "stop"]);
    });

    it("is a no-op in GENERATION_ENDED, FREE_RUNNING and CONFIRM", () => {
        const ended = create();
        ended.m.onWorkerReady();
        ended.m.toggle();
        ended.m.onGenComplete();
        ended.m.toggle();
        expect(ended.m.phase).toBe("GENERATION_ENDED");
        expect(types(ended.sent)).toEqual(["play"]);

        const free = create();
        free.m.onWorkerReady();
        free.m.toggleFreeRun();
        free.m.toggle();
        expect(free.m.phase).toBe("FREE_RUNNING");
        expect(types(free.sent)).toEqual(["startFreeRun"]);

        const confirm = create();
        confirm.m.onWorkerReady();
        confirm.m.toggle();
        confirm.m.setDraft(makeParams({ population: 1 }));
        confirm.m.toggle();
        confirm.m.toggle();
        expect(confirm.m.phase).toBe("CONFIRM");
        expect(types(confirm.sent)).toEqual(["play", "stop"]);
    });
});

describe("step", () => {
    it("moves WORKER_READY to STEPS_PAUSED and sends step", () => {
        const { m, sent } = create();
        m.onWorkerReady();
        m.step();
        expect(m.phase).toBe("STEPS_PAUSED");
        expect(types(sent)).toEqual(["step"]);
    });

    it("sends step from STEPS_PAUSED without changing phase", () => {
        const { m, sent } = create();
        m.onWorkerReady();
        m.step();
        m.step();
        expect(m.phase).toBe("STEPS_PAUSED");
        expect(types(sent)).toEqual(["step", "step"]);
    });

    it("is a no-op while running or free-running", () => {
        const { m, sent } = create();
        m.onWorkerReady();
        m.toggle();
        m.step();
        expect(types(sent)).toEqual(["play"]);
        m.toggle();
        m.toggleFreeRun();
        m.step();
        expect(types(sent)).toEqual(["play", "stop", "startFreeRun"]);
    });
});

describe("free run", () => {
    it("cycles FREE_RUNNING -> FREE_RUN_STOPPING -> STEPS_PAUSED", () => {
        const { m, sent } = create();
        m.onWorkerReady();
        m.toggleFreeRun();
        expect(m.phase).toBe("FREE_RUNNING");
        m.toggleFreeRun();
        expect(m.phase).toBe("FREE_RUN_STOPPING");
        m.toggleFreeRun(); // no-op while stopping
        expect(m.phase).toBe("FREE_RUN_STOPPING");
        m.onFreeRunPaused();
        expect(m.phase).toBe("STEPS_PAUSED");
        expect(types(sent)).toEqual(["startFreeRun", "stopFreeRun"]);
    });

    it("keeps free-running when the draft turns dirty (no configure sent)", () => {
        const { m, sent } = create();
        m.onWorkerReady();
        m.toggleFreeRun();
        m.setDraft(makeParams({ population: 1 }));
        expect(m.phase).toBe("FREE_RUNNING");
        expect(m.dirty).toBe(true);
        expect(types(sent)).toEqual(["startFreeRun"]);
    });

    it("onFreeRunPaused is a no-op outside FREE_RUN_STOPPING", () => {
        const { m } = create();
        m.onWorkerReady();
        m.toggle();
        m.onFreeRunPaused();
        expect(m.phase).toBe("STEPS_RUNNING");
    });
});

describe("onGenComplete", () => {
    it("transitions STEPS_RUNNING to GENERATION_ENDED", () => {
        const { m } = create();
        m.onWorkerReady();
        m.toggle();
        m.onGenComplete();
        expect(m.phase).toBe("GENERATION_ENDED");
    });

    it("leaves other phases unchanged", () => {
        const { m } = create();
        m.onWorkerReady();
        m.onGenComplete();
        expect(m.phase).toBe("WORKER_READY");
    });
});

describe("nextGen / rewind", () => {
    function toEnded() {
        const ctx = create();
        ctx.m.onWorkerReady();
        ctx.m.toggle();
        ctx.m.onGenComplete();
        ctx.sent.length = 0;
        return ctx;
    }

    it("clean without autoPlay respawns eagerly from GENERATION_ENDED", () => {
        const { m, sent } = toEnded();
        m.nextGen(false);
        expect(m.phase).toBe("GENERATION_SPAWNED");
        expect(sent).toEqual([{ type: "nextGeneration" }]);
    });

    it("clean with autoPlay runs and sends play after the gen command", () => {
        const { m, sent } = toEnded();
        m.rewind(true);
        expect(m.phase).toBe("STEPS_RUNNING");
        expect(types(sent)).toEqual(["rewind", "play"]);
    });

    it("dirty sends the *Configured variant and commits on the reply", () => {
        const { m, sent } = toEnded();
        const params = makeParams({ population: DEFAULTS.population + 1 });
        m.setDraft(params);
        m.nextGen(false);
        expect(m.phase).toBe("GENERATION_SPAWNED");
        expect(sent).toEqual([{ type: "nextGenerationConfigured", params }]);
        expect(m.dirty).toBe(true); // not committed until the reply
        m.onNextGenerationConfigured(0, 0);
        expect(m.dirty).toBe(false);
        expect(m.phase).toBe("GENERATION_SPAWNED");
    });

    it("dirty rewind with autoPlay keeps running through the reply", () => {
        const { m, sent } = toEnded();
        const params = makeParams({ population: DEFAULTS.population + 1 });
        m.setDraft(params);
        m.rewind(true);
        expect(m.phase).toBe("STEPS_RUNNING");
        expect(types(sent)).toEqual(["rewindConfigured", "play"]);
        m.onRewindConfigured();
        // The reply must not knock an autoPlay run back to spawned.
        expect(m.phase).toBe("STEPS_RUNNING");
        expect(m.dirty).toBe(false);
    });

    it("keeps STEPS_RUNNING when respawning mid-run without autoPlay", () => {
        const { m } = create();
        m.onWorkerReady();
        m.toggle();
        m.nextGen(false);
        expect(m.phase).toBe("STEPS_RUNNING");
        m.onNextGenerationConfigured(0, 0);
        expect(m.phase).toBe("STEPS_RUNNING");
    });

    it("is gated by genomIncompatible", () => {
        const { m, sent } = toEnded();
        m.setDraft(makeParams({ maxGenomeLen: 24 }));
        m.onCensus(32, 0);
        m.nextGen(false);
        m.rewind(false);
        expect(sent).toEqual([]);
        expect(m.phase).toBe("GENERATION_ENDED");
    });

    it("is a no-op while free-running or before the worker is ready", () => {
        const free = create();
        free.m.onWorkerReady();
        free.m.toggleFreeRun();
        free.m.nextGen(false);
        expect(types(free.sent)).toEqual(["startFreeRun"]);

        const pending = create();
        pending.m.rewind(false);
        expect(pending.sent).toEqual([]);
    });
});

describe("CONFIRM flow", () => {
    function toConfirm() {
        const ctx = create();
        ctx.m.onWorkerReady();
        ctx.m.toggle();
        ctx.m.setDraft(makeParams({ population: DEFAULTS.population + 1 }));
        ctx.m.toggle();
        ctx.sent.length = 0;
        return ctx;
    }

    it("confirmRevertContinue reverts the draft and resumes", () => {
        const { m, sent } = toConfirm();
        m.confirmRevertContinue();
        expect(m.phase).toBe("STEPS_RUNNING");
        expect(m.dirty).toBe(false);
        expect(types(sent)).toEqual(["play"]);
    });

    it("confirmRewind applies the dirty draft via rewindConfigured", () => {
        const { m, sent } = toConfirm();
        m.confirmRewind();
        expect(m.phase).toBe("GENERATION_SPAWNED");
        expect(types(sent)).toEqual(["rewindConfigured"]);
        // Dirty until the worker confirms the applied config.
        expect(m.dirty).toBe(true);
        m.onRewindConfigured();
        expect(m.dirty).toBe(false);
        expect(m.phase).toBe("GENERATION_SPAWNED");
    });

    it("confirm methods are no-ops outside CONFIRM", () => {
        const { m, sent } = create();
        m.onWorkerReady();
        m.confirmRevertContinue();
        m.confirmRewind();
        expect(m.phase).toBe("WORKER_READY");
        expect(sent).toEqual([]);
    });
});

describe("genome compatibility", () => {
    it("flags each truncating field reported by the census", () => {
        const { m } = create(makeParams({ maxGenomeLen: 24, maxNeurons: 8 }));
        m.onCensus(32, 12);
        expect(m.genomIncompatible).toBe(true);
        expect(m.incompatibleFields).toEqual(["maxGenomeLen", "maxNeurons"]);
        expect(m.requiredGenomeLen).toBe(32);
        expect(m.requiredNeurons).toBe(12);
    });

    it("clears when the draft is raised above the census values", () => {
        const { m } = create(makeParams({ maxGenomeLen: 24 }));
        m.onCensus(32, 0);
        m.setDraft(makeParams({ maxGenomeLen: 32 }));
        expect(m.genomIncompatible).toBe(false);
    });

    it("resets the counters on configure and rewind", () => {
        for (const reply of [
            (m: SimMachine) => m.onConfigured(),
            (m: SimMachine) => m.onRewindConfigured(),
        ]) {
            const { m } = create(makeParams({ maxGenomeLen: 24 }));
            m.onCensus(32, 12);
            reply(m);
            expect(m.requiredGenomeLen).toBe(0);
            expect(m.requiredNeurons).toBe(0);
            expect(m.genomIncompatible).toBe(false);
        }
    });

    it("takes the counters from the next-generation reply", () => {
        const { m } = create(makeParams({ maxGenomeLen: 24 }));
        m.onNextGenerationConfigured(32, 12);
        expect(m.requiredGenomeLen).toBe(32);
        expect(m.requiredNeurons).toBe(12);
        expect(m.genomIncompatible).toBe(true);
    });

    it("clearGenom resets the counters and notifies the worker", () => {
        const { m, sent } = create(makeParams({ maxGenomeLen: 24 }));
        m.onCensus(32, 12);
        m.clearGenom();
        expect(m.genomIncompatible).toBe(false);
        expect(m.requiredGenomeLen).toBe(0);
        expect(m.requiredNeurons).toBe(0);
        expect(types(sent)).toEqual(["clearGenom"]);
    });
});

describe("draft config", () => {
    it("setDraft / revertDraft drive the dirty derivation", () => {
        const { m } = create();
        expect(m.dirty).toBe(false);
        m.setDraft(makeParams({ population: 1 }));
        expect(m.dirty).toBe(true);
        m.revertDraft();
        expect(m.dirty).toBe(false);
        expect(m.draftConfig.population).toBe(DEFAULTS.population);
    });
});

describe("snapshots", () => {
    it("loadSnapshot transfers the buffer and lands in GENERATION_SPAWNED", () => {
        const { m, sent, transfers } = create();
        m.onWorkerReady();
        // A draft edit must not ride along the load command itself.
        m.setDraft(makeParams({ population: DEFAULTS.population + 1 }));
        const data = new Uint8Array([1, 2, 3]);
        m.loadSnapshot(data);
        expect(sent).toEqual([
            { type: "loadSnapshot", data, rewindFirst: false },
        ]);
        expect(transfers[0]).toEqual([data.buffer]);
        expect(m.phase).toBe("GENERATION_SPAWNED");
    });

    it("loadSnapshot sets rewindFirst when interrupting a run", () => {
        for (const [setup, rewindFirst] of [
            [(m: SimMachine) => m.toggle(), true], // STEPS_RUNNING
            [(m: SimMachine) => m.step(), true], // STEPS_PAUSED
            [(m: SimMachine) => m.onConfigured(), false], // GENERATION_SPAWNED
        ] as const) {
            const { m, sent } = create();
            m.onWorkerReady();
            setup(m);
            sent.length = 0;
            m.loadSnapshot(new Uint8Array([1]));
            expect(sent[0]).toMatchObject({
                type: "loadSnapshot",
                rewindFirst,
            });
        }
    });

    it("loadSnapshot is rejected before ready and during free-run", () => {
        const pending = create();
        pending.m.loadSnapshot(new Uint8Array([1])); // WORKER_PENDING
        expect(pending.sent).toEqual([]);

        const free = create();
        free.m.onWorkerReady();
        free.m.toggleFreeRun(); // FREE_RUNNING
        free.sent.length = 0;
        free.m.loadSnapshot(new Uint8Array([1]));
        expect(free.sent).toEqual([]);
    });

    it("onSnapshotLoaded breeds spawn 2 when the snapshot fits the draft", () => {
        const { m, sent } = create(makeParams({ maxGenomeLen: 64 }));
        m.onWorkerReady();
        m.loadSnapshot(new Uint8Array([1]));
        sent.length = 0;
        m.onSnapshotLoaded(40, 0);
        expect(m.phase).toBe("GENERATION_SPAWNED");
        expect(m.genomIncompatible).toBe(false);
        expect(types(sent)).toEqual(["rewind"]); // spawn 2
    });

    it("onSnapshotLoaded rides a dirty draft on rewindConfigured", () => {
        const { m, sent } = create(makeParams({ maxGenomeLen: 64 }));
        m.onWorkerReady();
        m.setDraft(makeParams({ maxGenomeLen: 64, population: 222 }));
        m.loadSnapshot(new Uint8Array([1]));
        sent.length = 0;
        m.onSnapshotLoaded(40, 0);
        expect(types(sent)).toEqual(["rewindConfigured"]);
    });

    it("onSnapshotLoaded holds and fires the gate when too large", () => {
        const { m, sent } = create(makeParams({ maxGenomeLen: 24 }));
        m.onWorkerReady();
        m.loadSnapshot(new Uint8Array([1]));
        sent.length = 0;
        m.onSnapshotLoaded(40, 0);
        expect(m.phase).toBe("GENERATION_SPAWNED");
        expect(m.genomIncompatible).toBe(true);
        expect(m.incompatibleFields).toContain("maxGenomeLen");
        expect(sent).toEqual([]); // no spawn 2
    });

    it("onSnapshotLoaded holds and fires the neuron gate when too many neurons", () => {
        const { m, sent } = create(
            makeParams({ maxGenomeLen: 64, maxNeurons: 5 }),
        );
        m.onWorkerReady();
        m.loadSnapshot(new Uint8Array([1]));
        sent.length = 0;
        m.onSnapshotLoaded(40, 8); // fits genome len, exceeds neurons
        expect(m.phase).toBe("GENERATION_SPAWNED");
        expect(m.genomIncompatible).toBe(true);
        expect(m.incompatibleFields).toContain("maxNeurons");
        expect(m.incompatibleFields).not.toContain("maxGenomeLen");
        expect(sent).toEqual([]); // no spawn 2
    });

    it("raising maxNeurons clears the gate, then Rewind breeds the snapshot", () => {
        const { m, sent } = create(
            makeParams({ maxGenomeLen: 64, maxNeurons: 5 }),
        );
        m.onWorkerReady();
        m.loadSnapshot(new Uint8Array([1]));
        m.onSnapshotLoaded(40, 8);
        expect(m.genomIncompatible).toBe(true);
        m.setDraft(makeParams({ maxGenomeLen: 64, maxNeurons: 8 }));
        expect(m.genomIncompatible).toBe(false);
        sent.length = 0;
        m.rewind(false);
        expect(types(sent)).toEqual(["rewindConfigured"]);
    });

    it("raising maxGenomeLen clears the gate, then Rewind breeds the snapshot", () => {
        const { m, sent } = create(makeParams({ maxGenomeLen: 24 }));
        m.onWorkerReady();
        m.loadSnapshot(new Uint8Array([1]));
        m.onSnapshotLoaded(40, 0);
        expect(m.genomIncompatible).toBe(true);
        m.setDraft(makeParams({ maxGenomeLen: 40 }));
        expect(m.genomIncompatible).toBe(false);
        sent.length = 0;
        m.rewind(false);
        expect(types(sent)).toEqual(["rewindConfigured"]);
    });

    it("exportSnapshot sends the export command", () => {
        const { m, sent } = create();
        m.exportSnapshot();
        expect(sent).toEqual([{ type: "exportSnapshot" }]);
    });
});
