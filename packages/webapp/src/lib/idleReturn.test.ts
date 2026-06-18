import { describe, it, expect } from "vitest";
import { shouldIdleReturn, type IdleReturnArgs } from "./idleReturn";

function args(overrides: Partial<IdleReturnArgs> = {}): IdleReturnArgs {
    return {
        mode: "running",
        playing: false,
        freeRunning: false,
        now: 10_000,
        lastActivity: 0,
        timeoutMs: 5_000,
        ...overrides,
    };
}

describe("shouldIdleReturn", () => {
    it("returns true at rest once the timeout has elapsed", () => {
        expect(shouldIdleReturn(args())).toBe(true);
    });

    it("returns false while still within the timeout", () => {
        expect(shouldIdleReturn(args({ now: 4_999, timeoutMs: 5_000 }))).toBe(
            false,
        );
    });

    it("returns false exactly at the timeout boundary", () => {
        expect(shouldIdleReturn(args({ now: 5_000, lastActivity: 0 }))).toBe(
            false,
        );
    });

    it("returns false while playing", () => {
        expect(shouldIdleReturn(args({ playing: true }))).toBe(false);
    });

    it("returns false while free-running", () => {
        expect(shouldIdleReturn(args({ freeRunning: true }))).toBe(false);
    });

    it("returns false when not showing the grid", () => {
        for (const mode of [
            "idle",
            "transitioning-in",
            "transitioning-out",
        ] as const) {
            expect(shouldIdleReturn(args({ mode }))).toBe(false);
        }
    });
});
