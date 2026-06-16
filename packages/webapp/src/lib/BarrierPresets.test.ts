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

    it("vertical split applies five vbars", () => {
        const barriers = clickPreset("Vertical split");
        expect(barriers).toHaveLength(5);
        expect(barriers.every((b) => b.kind === "vbar")).toBe(true);
    });

    it("bar cross applies four hbars and four vbars", () => {
        const barriers = clickPreset("Bar cross");
        expect(barriers).toHaveLength(8);
        expect(barriers.filter((b) => b.kind === "hbar")).toHaveLength(4);
        expect(barriers.filter((b) => b.kind === "vbar")).toHaveLength(4);
    });

    it("square applies four hbars and four vbars", () => {
        const barriers = clickPreset("Square");
        expect(barriers).toHaveLength(8);
        expect(barriers.filter((b) => b.kind === "hbar")).toHaveLength(4);
        expect(barriers.filter((b) => b.kind === "vbar")).toHaveLength(4);
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
