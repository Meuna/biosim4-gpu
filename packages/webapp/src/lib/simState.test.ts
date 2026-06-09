import { describe, it, expect } from "vitest";
import type { SimState } from "./simState";
import {
    SIM_STATES,
    enterDirty,
    exitDirty,
    onWorkerReady,
    onConfigured,
    onGenComplete,
    onFreeRunPaused,
    onRewindOrNextGenDone,
    onSnapshotLoaded,
} from "./simState";

describe("SIM_STATES", () => {
    it("contains 15 states", () => {
        expect(SIM_STATES.length).toBe(15);
    });
});

describe("enterDirty", () => {
    it("maps clean progression states to their DIRTY_* equivalents", () => {
        expect(enterDirty("GENERATION_SPAWNED")).toBe(
            "DIRTY_GENERATION_SPAWNED",
        );
        expect(enterDirty("STEPS_RUNNING")).toBe("DIRTY_STEPS_RUNNING");
        expect(enterDirty("STEPS_PAUSED")).toBe("DIRTY_STEPS_PAUSED");
        expect(enterDirty("GENERATION_ENDED")).toBe("DIRTY_GENERATION_ENDED");
        expect(enterDirty("FREE_RUNNING")).toBe("DIRTY_FREE_RUNNING");
        expect(enterDirty("FREE_RUN_STOPPING")).toBe("DIRTY_FREE_RUN_STOPPING");
    });

    it("leaves WORKER_PENDING and WORKER_READY unchanged", () => {
        expect(enterDirty("WORKER_PENDING")).toBe("WORKER_PENDING");
        expect(enterDirty("WORKER_READY")).toBe("WORKER_READY");
    });

    it("leaves already-dirty states unchanged", () => {
        expect(enterDirty("DIRTY_GENERATION_SPAWNED")).toBe(
            "DIRTY_GENERATION_SPAWNED",
        );
        expect(enterDirty("DIRTY_STEPS_RUNNING")).toBe("DIRTY_STEPS_RUNNING");
        expect(enterDirty("DIRTY_CONFIRM")).toBe("DIRTY_CONFIRM");
    });
});

describe("exitDirty", () => {
    it("maps DIRTY_* states to their clean equivalents", () => {
        expect(exitDirty("DIRTY_GENERATION_SPAWNED")).toBe(
            "GENERATION_SPAWNED",
        );
        expect(exitDirty("DIRTY_STEPS_RUNNING")).toBe("STEPS_RUNNING");
        expect(exitDirty("DIRTY_STEPS_PAUSED")).toBe("STEPS_PAUSED");
        expect(exitDirty("DIRTY_GENERATION_ENDED")).toBe("GENERATION_ENDED");
        expect(exitDirty("DIRTY_FREE_RUNNING")).toBe("FREE_RUNNING");
        expect(exitDirty("DIRTY_FREE_RUN_STOPPING")).toBe("FREE_RUN_STOPPING");
    });

    it("leaves DIRTY_CONFIRM unchanged (only explicit action can exit)", () => {
        expect(exitDirty("DIRTY_CONFIRM")).toBe("DIRTY_CONFIRM");
    });

    it("leaves clean states unchanged", () => {
        expect(exitDirty("WORKER_READY")).toBe("WORKER_READY");
        expect(exitDirty("STEPS_RUNNING")).toBe("STEPS_RUNNING");
        expect(exitDirty("GENERATION_ENDED")).toBe("GENERATION_ENDED");
    });

    it("enterDirty and exitDirty round-trip for all mappable states", () => {
        const mappable: SimState[] = [
            "GENERATION_SPAWNED",
            "STEPS_RUNNING",
            "STEPS_PAUSED",
            "GENERATION_ENDED",
            "FREE_RUNNING",
            "FREE_RUN_STOPPING",
        ];
        for (const s of mappable) {
            expect(exitDirty(enterDirty(s))).toBe(s);
        }
    });
});

describe("onWorkerReady", () => {
    it("transitions WORKER_PENDING to WORKER_READY", () => {
        expect(onWorkerReady("WORKER_PENDING")).toBe("WORKER_READY");
    });

    it("leaves any other state unchanged", () => {
        expect(onWorkerReady("WORKER_READY")).toBe("WORKER_READY");
        expect(onWorkerReady("STEPS_RUNNING")).toBe("STEPS_RUNNING");
    });
});

describe("onConfigured", () => {
    it("always returns GENERATION_SPAWNED", () => {
        expect(onConfigured()).toBe("GENERATION_SPAWNED");
    });
});

describe("onGenComplete", () => {
    it("transitions STEPS_RUNNING to GENERATION_ENDED", () => {
        expect(onGenComplete("STEPS_RUNNING")).toBe("GENERATION_ENDED");
    });

    it("transitions DIRTY_STEPS_RUNNING to DIRTY_GENERATION_ENDED", () => {
        expect(onGenComplete("DIRTY_STEPS_RUNNING")).toBe(
            "DIRTY_GENERATION_ENDED",
        );
    });

    it("leaves other states unchanged", () => {
        expect(onGenComplete("STEPS_PAUSED")).toBe("STEPS_PAUSED");
        expect(onGenComplete("FREE_RUNNING")).toBe("FREE_RUNNING");
    });
});

describe("onFreeRunPaused", () => {
    it("transitions FREE_RUN_STOPPING to STEPS_PAUSED", () => {
        expect(onFreeRunPaused("FREE_RUN_STOPPING")).toBe("STEPS_PAUSED");
    });

    it("transitions DIRTY_FREE_RUN_STOPPING to DIRTY_STEPS_PAUSED", () => {
        expect(onFreeRunPaused("DIRTY_FREE_RUN_STOPPING")).toBe(
            "DIRTY_STEPS_PAUSED",
        );
    });

    it("leaves other states unchanged", () => {
        expect(onFreeRunPaused("STEPS_RUNNING")).toBe("STEPS_RUNNING");
        expect(onFreeRunPaused("FREE_RUNNING")).toBe("FREE_RUNNING");
    });
});

describe("onRewindOrNextGenDone", () => {
    it("transitions GENERATION_ENDED to GENERATION_SPAWNED", () => {
        expect(onRewindOrNextGenDone("GENERATION_ENDED")).toBe(
            "GENERATION_SPAWNED",
        );
    });

    it("transitions DIRTY_GENERATION_ENDED to DIRTY_GENERATION_SPAWNED", () => {
        expect(onRewindOrNextGenDone("DIRTY_GENERATION_ENDED")).toBe(
            "DIRTY_GENERATION_SPAWNED",
        );
    });

    it("transitions DIRTY_CONFIRM to GENERATION_SPAWNED (dialog rewind)", () => {
        expect(onRewindOrNextGenDone("DIRTY_CONFIRM")).toBe(
            "GENERATION_SPAWNED",
        );
    });

    it("preserves STEPS_RUNNING when autoPlay was sent first", () => {
        expect(onRewindOrNextGenDone("STEPS_RUNNING")).toBe("STEPS_RUNNING");
        expect(onRewindOrNextGenDone("DIRTY_STEPS_RUNNING")).toBe(
            "DIRTY_STEPS_RUNNING",
        );
    });
});

describe("onSnapshotLoaded", () => {
    it("always returns STEPS_PAUSED", () => {
        expect(onSnapshotLoaded()).toBe("STEPS_PAUSED");
    });
});
