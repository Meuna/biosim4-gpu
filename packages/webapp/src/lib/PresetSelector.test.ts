import { render, screen, fireEvent } from "@testing-library/svelte";
import { vi } from "vitest";
import PresetSelector from "./PresetSelector.svelte";
import type { Preset } from "./presets";

function preset(id: string, name: string, hasSnapshot = false): Preset {
    return {
        id,
        name,
        hasSnapshot,
        loadConf: vi.fn(),
        loadSnapshot: vi.fn(),
    };
}

const PRESETS = [
    preset("default", "Default"),
    preset("corners", "Corners"),
    preset("evolved", "Evolved", true),
];

describe("PresetSelector", () => {
    it("shows the selected preset name", () => {
        render(PresetSelector, {
            props: {
                presets: PRESETS,
                selectedId: "corners",
                onSelect: vi.fn(),
            },
        });
        expect(screen.getByRole("button", { expanded: false })).toBeTruthy();
        expect(screen.getByText("Corners")).toBeTruthy();
    });

    it("opens the list on click and lists every preset", async () => {
        render(PresetSelector, {
            props: {
                presets: PRESETS,
                selectedId: "default",
                onSelect: vi.fn(),
            },
        });
        expect(screen.queryByRole("listbox")).toBeNull();
        await fireEvent.click(screen.getByRole("button", { expanded: false }));
        const options = screen.getAllByRole("option");
        expect(options.map((o) => o.textContent?.trim())).toEqual([
            "Default",
            "Corners",
            "Evolved",
        ]);
    });

    it("marks the selected option and calls onSelect with the chosen preset", async () => {
        const onSelect = vi.fn();
        render(PresetSelector, {
            props: { presets: PRESETS, selectedId: "default", onSelect },
        });
        await fireEvent.click(screen.getByRole("button", { expanded: false }));
        const corners = screen
            .getAllByRole("option")
            .find((o) => o.textContent?.includes("Corners"))!;
        await fireEvent.click(corners.querySelector("button")!);
        expect(onSelect).toHaveBeenCalledWith(
            expect.objectContaining({ id: "corners" }),
        );
        // The menu closes after a selection.
        expect(screen.queryByRole("listbox")).toBeNull();
    });

    it("closes on Escape", async () => {
        render(PresetSelector, {
            props: {
                presets: PRESETS,
                selectedId: "default",
                onSelect: vi.fn(),
            },
        });
        await fireEvent.click(screen.getByRole("button", { expanded: false }));
        expect(screen.getByRole("listbox")).toBeTruthy();
        await fireEvent.keyDown(window, { key: "Escape" });
        expect(screen.queryByRole("listbox")).toBeNull();
    });

    it("disables the trigger when disabled", () => {
        render(PresetSelector, {
            props: {
                presets: PRESETS,
                selectedId: "default",
                disabled: true,
                onSelect: vi.fn(),
            },
        });
        expect((screen.getByRole("button") as HTMLButtonElement).disabled).toBe(
            true,
        );
    });
});
