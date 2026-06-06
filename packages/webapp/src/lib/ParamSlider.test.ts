import { render, screen, fireEvent } from "@testing-library/svelte";
import { vi } from "vitest";
import ParamSlider from "./ParamSlider.svelte";

describe("ParamSlider", () => {
    it("renders label and current value", () => {
        render(ParamSlider, {
            props: {
                label: "Population",
                min: 100,
                max: 10000,
                step: 100,
                value: 3000,
                onchange: vi.fn(),
            },
        });
        expect(screen.getByText("Population")).toBeTruthy();
        expect(
            screen.getByRole("button", { name: "Edit Population" }),
        ).toBeTruthy();
    });

    it("renders hint when provided", () => {
        render(ParamSlider, {
            props: {
                label: "Population",
                hint: "agents",
                min: 100,
                max: 10000,
                step: 100,
                value: 3000,
                onchange: vi.fn(),
            },
        });
        expect(screen.getByText("agents")).toBeTruthy();
    });

    it("calls onchange when slider is committed", async () => {
        const onchange = vi.fn();
        render(ParamSlider, {
            props: {
                label: "Population",
                min: 100,
                max: 10000,
                step: 100,
                value: 3000,
                onchange,
            },
        });
        await fireEvent.change(screen.getByLabelText("Population"), {
            target: { value: "500" },
        });
        expect(onchange).toHaveBeenCalledWith(500);
    });

    it("shows a number input when value button is clicked", async () => {
        render(ParamSlider, {
            props: {
                label: "Population",
                min: 100,
                max: 10000,
                step: 100,
                value: 3000,
                onchange: vi.fn(),
            },
        });
        await fireEvent.click(
            screen.getByRole("button", { name: "Edit Population" }),
        );
        expect(screen.getByLabelText("Edit Population")).toBeTruthy();
    });

    it("commits value on Enter and returns to display mode", async () => {
        const onchange = vi.fn();
        render(ParamSlider, {
            props: {
                label: "Population",
                min: 100,
                max: 10000,
                step: 100,
                value: 3000,
                onchange,
            },
        });
        await fireEvent.click(
            screen.getByRole("button", { name: "Edit Population" }),
        );
        const input = screen.getByLabelText("Edit Population");
        await fireEvent.input(input, { target: { value: "500" } });
        await fireEvent.keyDown(input, { key: "Enter" });
        expect(onchange).toHaveBeenCalledWith(500);
        // The number input (spinbutton) is gone; the button returns to display mode.
        expect(screen.queryByRole("spinbutton")).toBeNull();
    });

    it("cancels edit on Escape without calling onchange", async () => {
        const onchange = vi.fn();
        render(ParamSlider, {
            props: {
                label: "Population",
                min: 100,
                max: 10000,
                step: 100,
                value: 3000,
                onchange,
            },
        });
        await fireEvent.click(
            screen.getByRole("button", { name: "Edit Population" }),
        );
        await fireEvent.keyDown(screen.getByLabelText("Edit Population"), {
            key: "Escape",
        });
        expect(onchange).not.toHaveBeenCalled();
        // The number input (spinbutton) is gone; the button returns to display mode.
        expect(screen.queryByRole("spinbutton")).toBeNull();
    });

    it("clamps out-of-range values to [min, max]", async () => {
        const onchange = vi.fn();
        render(ParamSlider, {
            props: {
                label: "Population",
                min: 100,
                max: 10000,
                step: 100,
                value: 3000,
                onchange,
            },
        });
        await fireEvent.click(
            screen.getByRole("button", { name: "Edit Population" }),
        );
        const input = screen.getByLabelText("Edit Population");
        await fireEvent.input(input, { target: { value: "99999" } });
        await fireEvent.keyDown(input, { key: "Enter" });
        expect(onchange).toHaveBeenCalledWith(10000);
    });

    it("uses custom format function for display", () => {
        render(ParamSlider, {
            props: {
                label: "Mutation rate",
                min: 0,
                max: 0.05,
                step: 0.0001,
                value: 0.001,
                format: (v) => v.toFixed(4),
                onchange: vi.fn(),
            },
        });
        expect(screen.getByText("0.0010")).toBeTruthy();
    });
});
