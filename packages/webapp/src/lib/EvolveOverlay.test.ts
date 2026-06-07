import { render, screen } from "@testing-library/svelte";
import EvolveOverlay from "./EvolveOverlay.svelte";

const defaultGeom = { x: 0, y: 0, w: 800, h: 600 };

describe("EvolveOverlay", () => {
    it("renders Evolving label", () => {
        render(EvolveOverlay, { geom: defaultGeom, gen: 0, stopping: false });
        expect(screen.getByText(/evolving/i)).toBeTruthy();
    });

    it("displays the generation number", () => {
        render(EvolveOverlay, { geom: defaultGeom, gen: 42, stopping: false });
        expect(screen.getByText("Gen: 42")).toBeTruthy();
    });

    it("shows stop message when stopping", () => {
        render(EvolveOverlay, { geom: defaultGeom, gen: 7, stopping: true });
        expect(screen.getByText(/stop request sent/i)).toBeTruthy();
    });

    it("does not show stop message when not stopping", () => {
        render(EvolveOverlay, { geom: defaultGeom, gen: 7, stopping: false });
        expect(screen.queryByText(/stop request sent/i)).toBeNull();
    });
});
