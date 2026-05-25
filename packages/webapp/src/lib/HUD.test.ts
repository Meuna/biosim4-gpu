import { render, screen } from "@testing-library/svelte";
import HUD from "./HUD.svelte";

describe("HUD", () => {
    it("renders Telemetry header and all stat labels", () => {
        render(HUD, {
            running: false,
            gen: 0,
            step: 0,
            stepsPerGen: 300,
            pop: 2048,
            survivalHistory: [],
        });
        expect(screen.getByLabelText("Simulation telemetry")).toBeTruthy();
        expect(screen.getByText("gen")).toBeTruthy();
        expect(screen.getByText("step")).toBeTruthy();
        expect(screen.getByText("pop")).toBeTruthy();
        expect(screen.getByText("fps")).toBeTruthy();
    });

    it("shows zero-padded gen and step values", () => {
        render(HUD, {
            running: false,
            gen: 7,
            step: 42,
            stepsPerGen: 300,
            pop: 2048,
            survivalHistory: [],
        });
        expect(screen.getByText("007")).toBeTruthy();
        expect(screen.getByText("042")).toBeTruthy();
    });

    it("shows -- for fps when idle and 60 when running", () => {
        const { unmount } = render(HUD, {
            running: false,
            gen: 0,
            step: 0,
            stepsPerGen: 300,
            pop: 0,
            survivalHistory: [],
        });
        expect(screen.getByText("--")).toBeTruthy();
        unmount();

        render(HUD, {
            running: true,
            gen: 0,
            step: 0,
            stepsPerGen: 300,
            pop: 0,
            survivalHistory: [],
        });
        expect(screen.getByText("60")).toBeTruthy();
    });

    it("renders sparkline SVG when survival history has entries", () => {
        render(HUD, {
            running: true,
            gen: 3,
            step: 10,
            stepsPerGen: 300,
            pop: 2048,
            survivalHistory: [0.4, 0.6, 0.7],
        });
        expect(screen.getByLabelText("Survival rate sparkline")).toBeTruthy();
    });
});
