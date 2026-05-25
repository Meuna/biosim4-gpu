import { render, screen, fireEvent } from "@testing-library/svelte";
import SimConfigPanel from "./SimConfigPanel.svelte";

describe("SimConfigPanel", () => {
    it("renders the Simulation heading and Configuration eyebrow", () => {
        render(SimConfigPanel, {});
        expect(screen.getByText("Simulation")).toBeTruthy();
        expect(screen.getByText("Configuration")).toBeTruthy();
    });

    it("renders grid size pill buttons", () => {
        render(SimConfigPanel, {});
        expect(screen.getByRole("button", { name: "64" })).toBeTruthy();
        expect(screen.getByRole("button", { name: "128" })).toBeTruthy();
        expect(screen.getByRole("button", { name: "256" })).toBeTruthy();
    });

    it('starts in sync (not dirty) and shows "✓ in sync"', () => {
        render(SimConfigPanel, {});
        expect(screen.getByText("✓ in sync")).toBeTruthy();
    });

    it("marks apply button as dirty after changing a parameter", () => {
        render(SimConfigPanel, {});
        fireEvent.click(screen.getByRole("button", { name: "64" }));
        expect(screen.getByText("apply & restart →")).toBeTruthy();
    });

    it("renders all challenge pill options", () => {
        render(SimConfigPanel, {});
        expect(screen.getByRole("button", { name: "Corners" })).toBeTruthy();
        expect(screen.getByRole("button", { name: "Predator" })).toBeTruthy();
    });
});
