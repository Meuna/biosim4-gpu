import { render, screen, fireEvent } from "@testing-library/svelte";
import { vi } from "vitest";
import GridSizeControl from "./GridSizeControl.svelte";

describe("GridSizeControl", () => {
    it("renders the Grid section label", () => {
        render(GridSizeControl, {
            props: { gridSizeX: 128, gridSizeY: 128, onchange: vi.fn() },
        });
        expect(screen.getByText("Grid")).toBeTruthy();
        expect(screen.getByText("grid.h")).toBeTruthy();
    });

    it("starts in preset mode and shows pill buttons", () => {
        render(GridSizeControl, {
            props: { gridSizeX: 128, gridSizeY: 128, onchange: vi.fn() },
        });
        expect(screen.getByRole("button", { name: "64" })).toBeTruthy();
        expect(screen.getByRole("button", { name: "128" })).toBeTruthy();
        expect(screen.getByRole("button", { name: "192" })).toBeTruthy();
        expect(screen.getByRole("button", { name: "256" })).toBeTruthy();
    });

    it("marks the active preset pill as pressed", () => {
        render(GridSizeControl, {
            props: { gridSizeX: 128, gridSizeY: 128, onchange: vi.fn() },
        });
        const btn = screen.getByRole("button", { name: "128" });
        expect(btn.getAttribute("aria-pressed")).toBe("true");
    });

    it("calls onchange(s, s) when a preset pill is clicked", async () => {
        const onchange = vi.fn();
        render(GridSizeControl, {
            props: { gridSizeX: 128, gridSizeY: 128, onchange },
        });
        await fireEvent.click(screen.getByRole("button", { name: "64" }));
        expect(onchange).toHaveBeenCalledWith(64, 64);
    });

    it("shows 'Custom →' button in preset mode", () => {
        render(GridSizeControl, {
            props: { gridSizeX: 128, gridSizeY: 128, onchange: vi.fn() },
        });
        expect(screen.getByRole("button", { name: "Custom →" })).toBeTruthy();
    });

    it("switches to custom mode when 'Custom →' is clicked", async () => {
        render(GridSizeControl, {
            props: { gridSizeX: 128, gridSizeY: 128, onchange: vi.fn() },
        });
        await fireEvent.click(screen.getByRole("button", { name: "Custom →" }));
        expect(screen.getByLabelText("Width (X)")).toBeTruthy();
        expect(screen.getByLabelText("Height (Y)")).toBeTruthy();
    });

    it("calls onchange with new X and old Y when X slider changes", async () => {
        const onchange = vi.fn();
        render(GridSizeControl, {
            props: { gridSizeX: 128, gridSizeY: 128, onchange },
        });
        await fireEvent.click(screen.getByRole("button", { name: "Custom →" }));
        await fireEvent.input(screen.getByLabelText("Width (X)"), {
            target: { value: "64" },
        });
        expect(onchange).toHaveBeenCalledWith(64, 128);
    });

    it("shows '← Preset' button in custom mode", async () => {
        render(GridSizeControl, {
            props: { gridSizeX: 128, gridSizeY: 128, onchange: vi.fn() },
        });
        await fireEvent.click(screen.getByRole("button", { name: "Custom →" }));
        expect(screen.getByRole("button", { name: "← Preset" })).toBeTruthy();
    });
});
