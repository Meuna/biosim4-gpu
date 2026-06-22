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
    it("reflects the selected preset in the select value", () => {
        render(PresetSelector, {
            props: {
                presets: PRESETS,
                selectedId: "corners",
                onSelect: vi.fn(),
            },
        });
        const select = screen.getByRole("combobox") as HTMLSelectElement;
        expect(select.value).toBe("corners");
    });

    it("lists every preset as a plain-text option", () => {
        render(PresetSelector, {
            props: {
                presets: PRESETS,
                selectedId: "default",
                onSelect: vi.fn(),
            },
        });
        const options = screen.getAllByRole("option");
        expect(options.map((o) => o.textContent?.trim())).toEqual([
            "Default",
            "Corners",
            "Evolved",
        ]);
    });

    it("calls onSelect with the chosen preset on change", async () => {
        const onSelect = vi.fn();
        render(PresetSelector, {
            props: { presets: PRESETS, selectedId: "default", onSelect },
        });
        await fireEvent.change(screen.getByRole("combobox"), {
            target: { value: "corners" },
        });
        expect(onSelect).toHaveBeenCalledWith(
            expect.objectContaining({ id: "corners" }),
        );
    });

    it("disables the select when disabled", () => {
        render(PresetSelector, {
            props: {
                presets: PRESETS,
                selectedId: "default",
                disabled: true,
                onSelect: vi.fn(),
            },
        });
        expect(
            (screen.getByRole("combobox") as HTMLSelectElement).disabled,
        ).toBe(true);
    });

    it("shows the DNA glyph only when the selected preset has a snapshot", () => {
        const { container, rerender } = render(PresetSelector, {
            props: {
                presets: PRESETS,
                selectedId: "default",
                onSelect: vi.fn(),
            },
        });
        expect(container.querySelector(".preset__dna")).toBeNull();

        rerender({
            presets: PRESETS,
            selectedId: "evolved",
            onSelect: vi.fn(),
        });
        expect(container.querySelector(".preset__dna")).not.toBeNull();
    });
});
