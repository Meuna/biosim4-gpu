import { render, screen, fireEvent } from "@testing-library/svelte";
import SpeedSlider from "./SpeedSlider.svelte";

const defaultProps = {
    value: 0 as const,
    onChange: () => {},
};

function getKnobLeft(container: HTMLElement): string {
    const knob = container.querySelector(".speed-slider__knob") as HTMLElement;
    return knob.style.left;
}

describe("SpeedSlider", () => {
    it("renders 4 stop buttons", () => {
        render(SpeedSlider, defaultProps);
        const btns = screen.getAllByRole("button");
        expect(btns).toHaveLength(4);
    });

    it("calls onChange with 1 when '1 fps' button clicked", async () => {
        let received: number | undefined;
        render(SpeedSlider, {
            ...defaultProps,
            onChange: (fps) => {
                received = fps;
            },
        });
        await fireEvent.click(
            screen.getByRole("button", { name: /set speed to 1 fps/i }),
        );
        expect(received).toBe(1);
    });

    it("calls onChange with 25 when '25 fps' button clicked", async () => {
        let received: number | undefined;
        render(SpeedSlider, {
            ...defaultProps,
            onChange: (fps) => {
                received = fps;
            },
        });
        await fireEvent.click(
            screen.getByRole("button", { name: /set speed to 25 fps/i }),
        );
        expect(received).toBe(25);
    });

    it("calls onChange with 50 when '50 fps' button clicked", async () => {
        let received: number | undefined;
        render(SpeedSlider, {
            ...defaultProps,
            onChange: (fps) => {
                received = fps;
            },
        });
        await fireEvent.click(
            screen.getByRole("button", { name: /set speed to 50 fps/i }),
        );
        expect(received).toBe(50);
    });

    it("calls onChange with 0 when unlimited button clicked", async () => {
        let received: number | undefined;
        render(SpeedSlider, {
            value: 1 as const,
            onChange: (fps) => {
                received = fps;
            },
        });
        await fireEvent.click(
            screen.getByRole("button", { name: /set speed to unlimited/i }),
        );
        expect(received).toBe(0);
    });

    it("knob is at left: 0% when value=1", () => {
        const { container } = render(SpeedSlider, {
            ...defaultProps,
            value: 1 as const,
        });
        expect(getKnobLeft(container)).toBe("0%");
    });

    it("knob is near left: 33.33% when value=25", () => {
        const { container } = render(SpeedSlider, {
            ...defaultProps,
            value: 25 as const,
        });
        const left = parseFloat(getKnobLeft(container));
        expect(left).toBeCloseTo(33.333, 1);
    });

    it("knob is near left: 66.67% when value=50", () => {
        const { container } = render(SpeedSlider, {
            ...defaultProps,
            value: 50 as const,
        });
        const left = parseFloat(getKnobLeft(container));
        expect(left).toBeCloseTo(66.667, 1);
    });

    it("knob is at left: 100% when value=0 (unlimited)", () => {
        const { container } = render(SpeedSlider, {
            ...defaultProps,
            value: 0 as const,
        });
        expect(getKnobLeft(container)).toBe("100%");
    });

    it("active stop has aria-pressed=true, others false", () => {
        render(SpeedSlider, { ...defaultProps, value: 25 as const });
        const btns = screen.getAllByRole("button") as HTMLButtonElement[];
        // stops: 1fps, 25fps, 50fps, ∞
        expect(btns[0].getAttribute("aria-pressed")).toBe("false");
        expect(btns[1].getAttribute("aria-pressed")).toBe("true");
        expect(btns[2].getAttribute("aria-pressed")).toBe("false");
        expect(btns[3].getAttribute("aria-pressed")).toBe("false");
    });
});
