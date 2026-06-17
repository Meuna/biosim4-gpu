import { render, screen, fireEvent } from "@testing-library/svelte";
import { vi } from "vitest";
import BarrierPresets from "./BarrierPresets.svelte";
import type { BarrierSpec } from "../workers/sim.worker";

function clickPreset(label: string): BarrierSpec[] {
    const onapply = vi.fn<[BarrierSpec[]], void>();
    render(BarrierPresets, { props: { onapply } });
    fireEvent.click(screen.getByLabelText(`${label} preset`));
    expect(onapply).toHaveBeenCalledOnce();
    return onapply.mock.calls[0][0];
}

describe("BarrierPresets", () => {
    it("renders a pill for every preset", () => {
        render(BarrierPresets, { props: { onapply: vi.fn() } });
        for (const label of [
            "Cross",
            "Vertical split",
            "Bar cross",
            "Square",
            "5 dots",
            "Random",
        ]) {
            expect(screen.getByLabelText(`${label} preset`)).toBeTruthy();
        }
    });

    it("cross applies one hbar and one vbar", () => {
        const barriers = clickPreset("Cross");
        expect(barriers).toHaveLength(2);
        expect(barriers.map((b) => b.kind).sort()).toEqual(["hbar", "vbar"]);
    });

    it("vertical split applies seven vbars", () => {
        const barriers = clickPreset("Vertical split");
        expect(barriers).toHaveLength(7);
        expect(barriers.every((b) => b.kind === "vbar")).toBe(true);
    });

    it("bar cross applies four corners, one per quadrant", () => {
        const barriers = clickPreset("Bar cross");
        expect(barriers).toHaveLength(4);
        expect(barriers.every((b) => b.kind === "corner")).toBe(true);
        expect(barriers.map((b) => b.quadrant).sort()).toEqual([
            "ne",
            "nw",
            "se",
            "sw",
        ]);
    });

    it("bar cross sits its corner junctions near the centre", () => {
        // Inner elbows: every junction sits on an inner row/column (~0.35 /
        // ~0.65), leaving an open plus through the middle. This discriminates
        // the bar-cross layout from the square box.
        const barriers = clickPreset("Bar cross");
        for (const b of barriers) {
            expect(b.x === 0.4 || b.x === 0.6).toBe(true);
            expect(b.y === 0.4 || b.y === 0.6).toBe(true);
        }
    });

    it("square applies four corners, one per quadrant", () => {
        const barriers = clickPreset("Square");
        expect(barriers).toHaveLength(4);
        expect(barriers.every((b) => b.kind === "corner")).toBe(true);
        expect(barriers.map((b) => b.quadrant).sort()).toEqual([
            "ne",
            "nw",
            "se",
            "sw",
        ]);
    });

    it("square hugs the outer perimeter", () => {
        // Junctions sit at the outer corners (~0.2 / ~0.8) with arms reaching
        // inward, so each side keeps a gap in the middle.
        const barriers = clickPreset("Square");
        for (const b of barriers) {
            expect(b.x === 0.15 || b.x === 0.85).toBe(true);
            expect(b.y === 0.15 || b.y === 0.85).toBe(true);
        }
    });

    it("five dots applies five circles", () => {
        const barriers = clickPreset("5 dots");
        expect(barriers).toHaveLength(5);
        expect(barriers.every((b) => b.kind === "circle")).toBe(true);
        expect(barriers.every((b) => b.width === null)).toBe(true);
    });

    it("random applies 10 to 20 circles", () => {
        const barriers = clickPreset("Random");
        expect(barriers.length).toBeGreaterThanOrEqual(10);
        expect(barriers.length).toBeLessThanOrEqual(20);
        expect(barriers.every((b) => b.kind === "circle")).toBe(true);
    });

    it("disables every pill when disabled", () => {
        render(BarrierPresets, { props: { disabled: true, onapply: vi.fn() } });
        expect(
            (screen.getByLabelText("Cross preset") as HTMLButtonElement)
                .disabled,
        ).toBe(true);
    });
});
