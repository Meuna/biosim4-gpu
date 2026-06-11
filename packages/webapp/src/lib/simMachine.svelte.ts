import type { SimParams, WorkerCmd } from "../workers/sim.worker";
import { DEFAULTS } from "./tomlConfig";

export const SIM_PHASES = [
    "WORKER_PENDING",
    "WORKER_READY",
    "GENERATION_SPAWNED",
    "STEPS_RUNNING",
    "STEPS_PAUSED",
    "GENERATION_ENDED",
    "FREE_RUNNING",
    "FREE_RUN_STOPPING",
    "CONFIRM",
] as const;

export type SimPhase = (typeof SIM_PHASES)[number];

export type SendFn = (cmd: WorkerCmd, transfer?: Transferable[]) => void;

// The webapp simulation state machine. Owns the composite state
// { phase, dirty }: `phase` is the simulation lifecycle position and `dirty`
// is derived from the divergence between the draft config and the config the
// worker last applied. Every phase transition and every lifecycle worker
// command goes through this class — UX gestures call the public UX methods,
// worker replies call the on* methods, and the rest of the app only reads the
// public getters.
//
// Invariants enforced by construction:
// - CONFIRM is entered only by toggle() from a dirty STEPS_RUNNING and exited
//   only by confirmRevertContinue() / confirmRewind().
// - A configure-then-play gesture is tracked in private pending context until
//   the worker reply commits it (FREE_RUN_STOPPING is the only awaiting-reply
//   condition modeled as a phase).
export class SimMachine {
    #phase = $state<SimPhase>("WORKER_PENDING");
    #draftConfig = $state<SimParams>(structuredClone(DEFAULTS));
    #lastPlayedConfig = $state<SimParams>(structuredClone(DEFAULTS));
    #genomeMaxLenUsed = $state(0);
    #genomeMaxNeuronsUsed = $state(0);

    // Pending context for an in-flight configure / configured-respawn command:
    // committed (or acted upon) when the worker reply arrives.
    #pendingPlay = false;
    #pendingConfig: SimParams | null = null;

    readonly #send: SendFn;

    // Raw draft/last-played divergence. Not masked by phase: a draft edited
    // before the first play is dirty even in WORKER_READY (the config panel
    // shows its revert affordance there too); phase checks gate everything
    // phase-specific.
    #dirty = $derived(
        JSON.stringify($state.snapshot(this.#draftConfig)) !==
            JSON.stringify($state.snapshot(this.#lastPlayedConfig)),
    );

    // Genome compatibility gate — true when the draft would truncate live
    // survivors reported by the last census.
    #genomIncompatible = $derived(
        (this.#genomeMaxLenUsed > 0 &&
            this.#draftConfig.maxGenomeLen < this.#genomeMaxLenUsed) ||
            (this.#genomeMaxNeuronsUsed > 0 &&
                this.#draftConfig.maxNeurons < this.#genomeMaxNeuronsUsed),
    );
    #incompatibleFields = $derived<string[]>([
        ...(this.#genomeMaxLenUsed > 0 &&
        this.#draftConfig.maxGenomeLen < this.#genomeMaxLenUsed
            ? ["maxGenomeLen"]
            : []),
        ...(this.#genomeMaxNeuronsUsed > 0 &&
        this.#draftConfig.maxNeurons < this.#genomeMaxNeuronsUsed
            ? ["maxNeurons"]
            : []),
    ]);

    constructor(send: SendFn, initialConfig: SimParams = DEFAULTS) {
        this.#send = send;
        this.#draftConfig = structuredClone(initialConfig);
        this.#lastPlayedConfig = structuredClone(initialConfig);
    }

    get phase(): SimPhase {
        return this.#phase;
    }

    get dirty(): boolean {
        return this.#dirty;
    }

    get draftConfig(): SimParams {
        return this.#draftConfig;
    }

    get genomIncompatible(): boolean {
        return this.#genomIncompatible;
    }

    get incompatibleFields(): string[] {
        return this.#incompatibleFields;
    }

    get genomeMaxLenUsed(): number {
        return this.#genomeMaxLenUsed;
    }

    get genomeMaxNeuronsUsed(): number {
        return this.#genomeMaxNeuronsUsed;
    }

    // ── UX methods ───────────────────────────────────────────────────────────

    // Play/Stop button. Stopping a dirty run enters CONFIRM (the config-change
    // dialog); starting with a dirty draft first sends configure and defers
    // play until onConfigured().
    toggle(): void {
        if (this.#phase === "STEPS_RUNNING") {
            this.#phase = this.#dirty ? "CONFIRM" : "STEPS_PAUSED";
            this.#send({ type: "stop" });
            return;
        }
        if (this.#genomIncompatible) return;
        if (
            this.#phase !== "WORKER_READY" &&
            this.#phase !== "GENERATION_SPAWNED" &&
            this.#phase !== "STEPS_PAUSED"
        ) {
            return;
        }
        if (this.#dirty) {
            this.#pendingConfig = this.#snapshotDraft();
            this.#pendingPlay = true;
            this.#send({ type: "configure", params: this.#pendingConfig });
        } else {
            this.#phase = "STEPS_RUNNING";
            this.#send({ type: "play" });
        }
    }

    step(): void {
        if (
            this.#phase !== "WORKER_READY" &&
            this.#phase !== "GENERATION_SPAWNED" &&
            this.#phase !== "STEPS_PAUSED" &&
            this.#phase !== "GENERATION_ENDED"
        ) {
            return;
        }
        if (this.#phase === "WORKER_READY") this.#phase = "STEPS_PAUSED";
        this.#send({ type: "step" });
    }

    toggleFreeRun(): void {
        if (this.#phase === "FREE_RUNNING") {
            this.#phase = "FREE_RUN_STOPPING";
            this.#send({ type: "stopFreeRun" });
            return;
        }
        if (
            this.#phase === "WORKER_READY" ||
            this.#phase === "GENERATION_SPAWNED" ||
            this.#phase === "STEPS_PAUSED" ||
            this.#phase === "GENERATION_ENDED"
        ) {
            this.#phase = "FREE_RUNNING";
            this.#send({ type: "startFreeRun" });
        }
    }

    nextGen(autoPlay: boolean): void {
        if (this.#genomIncompatible || !this.#canRespawn()) return;
        this.#respawn("nextGeneration", autoPlay);
    }

    rewind(autoPlay: boolean): void {
        if (this.#genomIncompatible || !this.#canRespawn()) return;
        this.#respawn("rewind", autoPlay);
    }

    // Config-change dialog: revert the draft and resume the stopped run.
    confirmRevertContinue(): void {
        if (this.#phase !== "CONFIRM") return;
        this.revertDraft();
        this.#phase = "STEPS_RUNNING";
        this.#send({ type: "play" });
    }

    // Config-change dialog: rewind applying the pending (dirty) draft.
    confirmRewind(): void {
        if (this.#phase !== "CONFIRM") return;
        this.#respawn("rewind", false);
    }

    setDraft(params: SimParams): void {
        this.#draftConfig = params;
    }

    revertDraft(): void {
        this.#draftConfig = $state.snapshot(
            this.#lastPlayedConfig,
        ) as SimParams;
    }

    clearGenom(): void {
        this.#genomeMaxLenUsed = 0;
        this.#genomeMaxNeuronsUsed = 0;
        this.#send({ type: "clearGenom" });
    }

    // Import survivors into the live sim. Carries only the population data —
    // never config: the worker imports into whatever the sim is currently
    // configured with, leaving grid/challenge/genome limits untouched.
    loadSnapshot(data: Uint8Array): void {
        this.#send({ type: "loadSnapshot", data }, [
            data.buffer as ArrayBuffer,
        ]);
    }

    exportSnapshot(): void {
        this.#send({ type: "exportSnapshot" });
    }

    // ── Worker-event methods ─────────────────────────────────────────────────

    onWorkerReady(): void {
        if (this.#phase === "WORKER_PENDING") this.#phase = "WORKER_READY";
    }

    onConfigured(): void {
        this.#phase = "GENERATION_SPAWNED";
        this.#commitPendingConfig();
        this.#resetGenomeUsed();
        if (this.#pendingPlay) {
            this.#pendingPlay = false;
            this.#phase = "STEPS_RUNNING";
            this.#send({ type: "play" });
        }
    }

    onGenComplete(): void {
        if (this.#phase === "STEPS_RUNNING") this.#phase = "GENERATION_ENDED";
    }

    onFreeRunPaused(): void {
        if (this.#phase === "FREE_RUN_STOPPING") this.#phase = "STEPS_PAUSED";
    }

    onRewindConfigured(): void {
        this.#onSpawnConfigured();
        this.#resetGenomeUsed();
    }

    // The next-generation reply carries the census of the generation that just
    // reproduced, so the counters are set rather than reset.
    onNextGenerationConfigured(
        genomeMaxLenUsed: number,
        genomeMaxNeuronsUsed: number,
    ): void {
        this.#onSpawnConfigured();
        this.#genomeMaxLenUsed = genomeMaxLenUsed;
        this.#genomeMaxNeuronsUsed = genomeMaxNeuronsUsed;
    }

    onSnapshotLoaded(): void {
        this.#phase = "STEPS_PAUSED";
        this.#commitPendingConfig();
        this.#resetGenomeUsed();
    }

    onCensus(genomeMaxLenUsed: number, genomeMaxNeuronsUsed: number): void {
        this.#genomeMaxLenUsed = genomeMaxLenUsed;
        this.#genomeMaxNeuronsUsed = genomeMaxNeuronsUsed;
    }

    // ── Private helpers ──────────────────────────────────────────────────────

    #resetGenomeUsed(): void {
        this.#genomeMaxLenUsed = 0;
        this.#genomeMaxNeuronsUsed = 0;
    }

    #snapshotDraft(): SimParams {
        return $state.snapshot(this.#draftConfig) as SimParams;
    }

    #commitPendingConfig(): void {
        if (this.#pendingConfig !== null) {
            this.#lastPlayedConfig = this.#pendingConfig;
            this.#pendingConfig = null;
        }
    }

    #canRespawn(): boolean {
        return (
            this.#phase !== "WORKER_PENDING" &&
            this.#phase !== "FREE_RUNNING" &&
            this.#phase !== "FREE_RUN_STOPPING" &&
            this.#phase !== "CONFIRM"
        );
    }

    // Shared Next Gen / Rewind gesture. A dirty draft rides along on the
    // *Configured command variant and is committed when the reply arrives.
    #respawn(kind: "nextGeneration" | "rewind", autoPlay: boolean): void {
        if (this.#dirty) {
            this.#pendingConfig = this.#snapshotDraft();
            this.#send(
                kind === "rewind"
                    ? { type: "rewindConfigured", params: this.#pendingConfig }
                    : {
                          type: "nextGenerationConfigured",
                          params: this.#pendingConfig,
                      },
            );
        } else {
            this.#send({ type: kind });
        }
        if (autoPlay) {
            this.#phase = "STEPS_RUNNING";
            this.#send({ type: "play" });
        } else if (
            this.#phase === "GENERATION_ENDED" ||
            this.#phase === "CONFIRM"
        ) {
            // Eager transition; the worker reply confirms it.
            this.#phase = "GENERATION_SPAWNED";
        }
    }

    // Reply to rewindConfigured / nextGenerationConfigured. If autoPlay
    // already moved the machine to STEPS_RUNNING before this reply arrived,
    // the running phase is preserved.
    #onSpawnConfigured(): void {
        this.#commitPendingConfig();
        if (this.#phase === "GENERATION_ENDED" || this.#phase === "CONFIRM") {
            this.#phase = "GENERATION_SPAWNED";
        }
    }
}
