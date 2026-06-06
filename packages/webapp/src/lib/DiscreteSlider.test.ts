import { render, screen, fireEvent } from "@testing-library/svelte";
import DiscreteSlider from "./DiscreteSlider.svelte";

const SPEED_STOPS = [
    { value: 1, label: "1", ariaLabel: "1 fps" },
    { value: 25, label: "25", ariaLabel: "25 fps" },
    { value: 50, label: "50", ariaLabel: "50 fps" },
    { value: 0, label: ">>", ariaLabel: "Unlimited" },
];

const defaultProps = {
    stops: SPEED_STOPS,
    value: 0,
    onChange: () => {},
    ariaLabel: "Simulation speed",
};

function getKnobLeft(container: HTMLElement): string {
    const knob = container.querySelector(
        ".discrete-slider__knob",
    ) as HTMLElement;
    return knob.style.left;
}

describe("DiscreteSlider", () => {
    it("renders 4 radio buttons", () => {
        render(DiscreteSlider, defaultProps);
        const radios = screen.getAllByRole("radio");
        expect(radios).toHaveLength(4);
    });

    it("renders with a radiogroup role and aria-label", () => {
        render(DiscreteSlider, defaultProps);
        expect(
            screen.getByRole("radiogroup", { name: /simulation speed/i }),
        ).toBeTruthy();
    });

    it("calls onChange with 1 when '1 fps' radio clicked", async () => {
        let received: number | undefined;
        render(DiscreteSlider, {
            ...defaultProps,
            onChange: (v) => {
                received = v;
            },
        });
        await fireEvent.click(screen.getByRole("radio", { name: /1 fps/i }));
        expect(received).toBe(1);
    });

    it("calls onChange with 25 when '25 fps' radio clicked", async () => {
        let received: number | undefined;
        render(DiscreteSlider, {
            ...defaultProps,
            onChange: (v) => {
                received = v;
            },
        });
        await fireEvent.click(screen.getByRole("radio", { name: /25 fps/i }));
        expect(received).toBe(25);
    });

    it("calls onChange with 50 when '50 fps' radio clicked", async () => {
        let received: number | undefined;
        render(DiscreteSlider, {
            ...defaultProps,
            onChange: (v) => {
                received = v;
            },
        });
        await fireEvent.click(screen.getByRole("radio", { name: /50 fps/i }));
        expect(received).toBe(50);
    });

    it("calls onChange with 0 when 'Unlimited' radio clicked", async () => {
        let received: number | undefined;
        render(DiscreteSlider, {
            ...defaultProps,
            value: 1,
            onChange: (v) => {
                received = v;
            },
        });
        await fireEvent.click(
            screen.getByRole("radio", { name: /unlimited/i }),
        );
        expect(received).toBe(0);
    });

    it("knob is at left: 0% when value=1", () => {
        const { container } = render(DiscreteSlider, {
            ...defaultProps,
            value: 1,
        });
        expect(getKnobLeft(container)).toBe("0%");
    });

    it("knob is near left: 33.33% when value=25", () => {
        const { container } = render(DiscreteSlider, {
            ...defaultProps,
            value: 25,
        });
        const left = parseFloat(getKnobLeft(container));
        expect(left).toBeCloseTo(33.333, 1);
    });

    it("knob is near left: 66.67% when value=50", () => {
        const { container } = render(DiscreteSlider, {
            ...defaultProps,
            value: 50,
        });
        const left = parseFloat(getKnobLeft(container));
        expect(left).toBeCloseTo(66.667, 1);
    });

    it("knob is at left: 100% when value=0 (unlimited)", () => {
        const { container } = render(DiscreteSlider, defaultProps);
        expect(getKnobLeft(container)).toBe("100%");
    });

    it("active radio has aria-checked=true, others false", () => {
        render(DiscreteSlider, { ...defaultProps, value: 25 });
        const radios = screen.getAllByRole("radio") as HTMLButtonElement[];
        expect(radios[0].getAttribute("aria-checked")).toBe("false");
        expect(radios[1].getAttribute("aria-checked")).toBe("true");
        expect(radios[2].getAttribute("aria-checked")).toBe("false");
        expect(radios[3].getAttribute("aria-checked")).toBe("false");
    });

    it("stop circles are rendered for each stop", () => {
        const { container } = render(DiscreteSlider, defaultProps);
        const circles = container.querySelectorAll(".discrete-slider__circle");
        expect(circles).toHaveLength(4);
    });
});
