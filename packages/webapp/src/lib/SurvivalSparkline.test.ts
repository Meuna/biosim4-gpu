import { render, screen } from "@testing-library/svelte";
import SurvivalSparkline from "./SurvivalSparkline.svelte";

const base = { min: 0.2, current: 0.7, max: 0.8, left: 80, maxWidth: 600 };

describe("SurvivalSparkline", () => {
    it("renders the sparkline SVG when survival history has entries", () => {
        render(SurvivalSparkline, {
            ...base,
            survivalHistory: [0.4, 0.6, 0.7],
        });
        expect(screen.getByLabelText("Survival rate sparkline")).toBeTruthy();
    });

    it("shows a dash placeholder when survival history is empty", () => {
        render(SurvivalSparkline, {
            ...base,
            survivalHistory: [],
            min: null,
            current: null,
            max: null,
        });
        expect(screen.queryByLabelText("Survival rate sparkline")).toBeNull();
    });

    it("labels the aside as Survival history", () => {
        render(SurvivalSparkline, { ...base, survivalHistory: [] });
        expect(screen.getByLabelText("Survival history")).toBeTruthy();
    });

    it("renders the min/now/max readout as integer percent", () => {
        render(SurvivalSparkline, {
            ...base,
            survivalHistory: [0.4, 0.7],
        });
        expect(screen.getByText(/min 20%/)).toBeTruthy();
        expect(screen.getByText(/now 70%/)).toBeTruthy();
        expect(screen.getByText(/max 80%/)).toBeTruthy();
    });

    it("shows dashes in the readout before any data arrives", () => {
        render(SurvivalSparkline, {
            ...base,
            survivalHistory: [],
            min: null,
            current: null,
            max: null,
        });
        expect(screen.getByText(/min —/)).toBeTruthy();
        expect(screen.getByText(/now —/)).toBeTruthy();
        expect(screen.getByText(/max —/)).toBeTruthy();
    });

    it("positions the aside with the supplied left and max-width", () => {
        const { container } = render(SurvivalSparkline, {
            ...base,
            survivalHistory: [0.4, 0.6],
            left: 44,
            maxWidth: 320,
        });
        const aside = container.querySelector(".hud") as HTMLElement;
        expect(aside.style.left).toBe("44px");
        expect(aside.style.maxWidth).toBe("320px");
    });

    it("grows the sparkline width with the generation count while it fits", () => {
        const { container } = render(SurvivalSparkline, {
            ...base,
            survivalHistory: [0.4, 0.6, 0.7], // 3 gens → (3-1)*7 = 14px
            maxWidth: 600,
        });
        const svg = container.querySelector(".hud__sparkline") as SVGElement;
        expect(svg.getAttribute("width")).toBe("14");
    });

    it("clamps the sparkline width to maxWidth once the dots overflow", () => {
        const { container } = render(SurvivalSparkline, {
            ...base,
            survivalHistory: Array.from({ length: 50 }, (_, i) => i / 49),
            maxWidth: 100, // (50-1)*7 = 343 > 100 → clamps
        });
        const svg = container.querySelector(".hud__sparkline") as SVGElement;
        expect(svg.getAttribute("width")).toBe("100");
    });

    it("draws the accent current dot pinned to the right edge", () => {
        const { container } = render(SurvivalSparkline, {
            ...base,
            survivalHistory: [0.4, 0.6, 0.7],
            maxWidth: 600,
        });
        const accentDot = container.querySelector(
            'circle[fill="var(--color-accent)"]',
        ) as SVGCircleElement;
        expect(accentDot).toBeTruthy();
        expect(accentDot.getAttribute("cx")).toBe("14");
    });
});
