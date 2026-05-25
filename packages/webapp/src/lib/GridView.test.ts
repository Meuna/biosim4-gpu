import { render, screen } from "@testing-library/svelte";
import GridView from "./GridView.svelte";

const defaultGeom = { x: 80, y: 136, size: 600, cx: 380, cy: 436 };

describe("GridView", () => {
    it("shows the idle overlay when not running", () => {
        render(GridView, { geom: defaultGeom, running: false });
        expect(screen.getByLabelText("Simulation not started")).toBeTruthy();
        expect(screen.getByText(/press play/i)).toBeTruthy();
    });

    it("hides the idle overlay when running", () => {
        render(GridView, { geom: defaultGeom, running: true });
        expect(screen.queryByLabelText("Simulation not started")).toBeNull();
        expect(screen.queryByText(/press play/i)).toBeNull();
    });

    it("shows the correct grid size in the idle meta line", () => {
        render(GridView, { geom: defaultGeom, running: false, gridSize: 64 });
        expect(screen.getByText(/64 × 64/)).toBeTruthy();
    });

    it("renders axis labels", () => {
        render(GridView, { geom: defaultGeom, running: false, gridSize: 128 });
        const labels = screen.getAllByText("128");
        expect(labels.length).toBeGreaterThanOrEqual(2);
    });
});
