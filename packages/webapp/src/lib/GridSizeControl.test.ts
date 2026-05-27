import { render, screen, fireEvent } from "@testing-library/svelte";
import { vi } from "vitest";
import GridSizeControl from "./GridSizeControl.svelte";

describe("GridSizeControl", () => {
    it("shows all four preset pill buttons", () => {
        render(GridSizeControl, {
            props: { gridSizeX: 128, gridSizeY: 128, onchange: vi.fn() },
        });
        expect(screen.getByRole("button", { name: "64" })).toBeTruthy();
        expect(screen.getByRole("button", { name: "128" })).toBeTruthy();
        expect(screen.getByRole("button", { name: "192" })).toBeTruthy();
        expect(screen.getByRole("button", { name: "256" })).toBeTruthy();
    });

    it("marks the matching preset as pressed when both dimensions match", () => {
        render(GridSizeControl, {
            props: { gridSizeX: 128, gridSizeY: 128, onchange: vi.fn() },
        });
        expect(
            screen
                .getByRole("button", { name: "128" })
                .getAttribute("aria-pressed"),
        ).toBe("true");
        expect(
            screen
                .getByRole("button", { name: "64" })
                .getAttribute("aria-pressed"),
        ).toBe("false");
    });

    it("shows no preset as active when X and Y differ", () => {
        render(GridSizeControl, {
            props: { gridSizeX: 64, gridSizeY: 128, onchange: vi.fn() },
        });
        for (const name of ["64", "128", "192", "256"]) {
            expect(
                screen
                    .getByRole("button", { name })
                    .getAttribute("aria-pressed"),
            ).toBe("false");
        }
    });

    it("calls onchange(s, s) when a preset pill is clicked", async () => {
        const onchange = vi.fn();
        render(GridSizeControl, {
            props: { gridSizeX: 128, gridSizeY: 128, onchange },
        });
        await fireEvent.click(screen.getByRole("button", { name: "64" }));
        expect(onchange).toHaveBeenCalledWith(64, 64);
    });

    it("always shows Width (X) and Height (Y) sliders", () => {
        render(GridSizeControl, {
            props: { gridSizeX: 128, gridSizeY: 128, onchange: vi.fn() },
        });
        expect(screen.getByLabelText("Width (X)")).toBeTruthy();
        expect(screen.getByLabelText("Height (Y)")).toBeTruthy();
    });

    it("calls onchange with new X and old Y when X slider changes", async () => {
        const onchange = vi.fn();
        render(GridSizeControl, {
            props: { gridSizeX: 128, gridSizeY: 128, onchange },
        });
        await fireEvent.input(screen.getByLabelText("Width (X)"), {
            target: { value: "64" },
        });
        expect(onchange).toHaveBeenCalledWith(64, 128);
    });

    it("calls onchange with old X and new Y when Y slider changes", async () => {
        const onchange = vi.fn();
        render(GridSizeControl, {
            props: { gridSizeX: 128, gridSizeY: 128, onchange },
        });
        await fireEvent.input(screen.getByLabelText("Height (Y)"), {
            target: { value: "64" },
        });
        expect(onchange).toHaveBeenCalledWith(128, 64);
    });
});
