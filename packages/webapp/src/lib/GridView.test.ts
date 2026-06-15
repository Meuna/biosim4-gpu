import { render, screen } from "@testing-library/svelte";
import GridView from "./GridView.svelte";

const defaultGeom = { x: 80, y: 136, w: 600, h: 600, cx: 380, cy: 436 };

describe("GridView", () => {
    it("shows the idle overlay when state is WORKER_PENDING", () => {
        render(GridView, {
            geom: defaultGeom,
            phase: "WORKER_PENDING" as const,
        });
        expect(screen.getByLabelText("Simulation not started")).toBeTruthy();
        expect(screen.getByText(/press play/i)).toBeTruthy();
    });

    it("shows the idle overlay when state is WORKER_READY", () => {
        render(GridView, {
            geom: defaultGeom,
            phase: "WORKER_READY" as const,
        });
        expect(screen.getByLabelText("Simulation not started")).toBeTruthy();
        expect(screen.getByText(/press play/i)).toBeTruthy();
    });

    it("hides the idle overlay when running", () => {
        render(GridView, {
            geom: defaultGeom,
            phase: "STEPS_RUNNING" as const,
        });
        expect(screen.queryByLabelText("Simulation not started")).toBeNull();
        expect(screen.queryByText(/press play/i)).toBeNull();
    });

    it("hides the idle overlay when paused", () => {
        render(GridView, {
            geom: defaultGeom,
            phase: "STEPS_PAUSED" as const,
        });
        expect(screen.queryByLabelText("Simulation not started")).toBeNull();
        expect(screen.queryByText(/press play/i)).toBeNull();
    });

    it("hides the idle overlay when generation complete", () => {
        render(GridView, {
            geom: defaultGeom,
            phase: "GENERATION_ENDED" as const,
        });
        expect(screen.queryByLabelText("Simulation not started")).toBeNull();
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
