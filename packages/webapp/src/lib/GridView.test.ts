import { fireEvent, render, screen } from "@testing-library/svelte";
import { vi } from "vitest";
import GridView from "./GridView.svelte";

const defaultGeom = { x: 80, y: 136, w: 600, h: 600, cx: 380, cy: 436 };

describe("GridView", () => {
    it("shows the idle overlay with a clickable play button on initial load", () => {
        render(GridView, {
            geom: defaultGeom,
            phase: "WORKER_READY" as const,
        });
        expect(screen.getByLabelText("Simulation not started")).toBeTruthy();
        expect(screen.getByRole("button", { name: /play/i })).toBeTruthy();
    });

    it("calls onPlay when the play button is clicked", async () => {
        const onPlay = vi.fn();
        render(GridView, {
            geom: defaultGeom,
            phase: "WORKER_READY" as const,
            onPlay,
        });
        await fireEvent.click(screen.getByRole("button", { name: /play/i }));
        expect(onPlay).toHaveBeenCalledTimes(1);
    });

    it("hides the idle overlay when returning to the sculpture after a generation", () => {
        render(GridView, {
            geom: defaultGeom,
            phase: "GENERATION_ENDED" as const,
        });
        expect(screen.queryByLabelText("Simulation not started")).toBeNull();
        expect(screen.queryByText(/press play/i)).toBeNull();
    });

    it("renders axis labels with separate X and Y values", () => {
        render(GridView, {
            geom: { ...defaultGeom, h: 300 },
            phase: "WORKER_READY" as const,
            gridSizeX: 256,
            gridSizeY: 128,
        });
        expect(screen.getByText("256")).toBeTruthy();
        expect(screen.getByText("128")).toBeTruthy();
    });
});
