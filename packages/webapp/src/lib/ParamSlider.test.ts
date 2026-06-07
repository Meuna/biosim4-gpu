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

    it("does NOT call onchange on input (only on commit)", async () => {
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
        await fireEvent.input(screen.getByLabelText("Population"), {
            target: { value: "500" },
        });
        expect(onchange).not.toHaveBeenCalled();
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

    it("accepts out-of-range values without clamping", async () => {
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
        expect(onchange).toHaveBeenCalledWith(99999);
    });

    it("applies muted class to range input when value is above max", () => {
        const { container } = render(ParamSlider, {
            props: {
                label: "Population",
                min: 100,
                max: 10000,
                step: 100,
                value: 99999,
                onchange: vi.fn(),
            },
        });
        const rangeInput = container.querySelector(".param-slider__range");
        expect(
            rangeInput?.classList.contains("param-slider__range--muted"),
        ).toBe(true);
    });

    it("applies muted class to value button when value is above max", () => {
        render(ParamSlider, {
            props: {
                label: "Population",
                min: 100,
                max: 10000,
                step: 100,
                value: 99999,
                onchange: vi.fn(),
            },
        });
        const btn = screen.getByRole("button", { name: "Edit Population" });
        expect(btn.classList.contains("param-slider__val--muted")).toBe(true);
    });

    it("applies muted class to range input when value is below min", () => {
        const { container } = render(ParamSlider, {
            props: {
                label: "Population",
                min: 100,
                max: 10000,
                step: 100,
                value: 1,
                onchange: vi.fn(),
            },
        });
        const rangeInput = container.querySelector(".param-slider__range");
        expect(
            rangeInput?.classList.contains("param-slider__range--muted"),
        ).toBe(true);
    });

    it("does not apply muted class when value is within range", () => {
        const { container } = render(ParamSlider, {
            props: {
                label: "Population",
                min: 100,
                max: 10000,
                step: 100,
                value: 3000,
                onchange: vi.fn(),
            },
        });
        const rangeInput = container.querySelector(".param-slider__range");
        expect(
            rangeInput?.classList.contains("param-slider__range--muted"),
        ).toBe(false);
    });

    it("removes muted class after slider input with in-range value", async () => {
        const { container } = render(ParamSlider, {
            props: {
                label: "Population",
                min: 100,
                max: 10000,
                step: 100,
                value: 99999,
                onchange: vi.fn(),
            },
        });
        const rangeInput = container.querySelector(
            ".param-slider__range",
        ) as HTMLInputElement;
        expect(
            rangeInput.classList.contains("param-slider__range--muted"),
        ).toBe(true);
        await fireEvent.input(rangeInput, { target: { value: "5000" } });
        expect(
            rangeInput.classList.contains("param-slider__range--muted"),
        ).toBe(false);
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
