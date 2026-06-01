import { render, screen } from "@testing-library/svelte";
import TelemetryHUD from "./TelemetryHUD.svelte";

const defaultGeom = { x: 80, y: 136, w: 600, h: 600, cx: 380, cy: 436 };

describe("TelemetryHUD", () => {
    it("renders the Simulation telemetry aria-label", () => {
        render(TelemetryHUD, {
            geom: defaultGeom,
            running: false,
            gen: 0,
            step: 0,
            stepsPerGen: 300,
            pop: 0,
        });
        expect(screen.getByLabelText("Simulation telemetry")).toBeTruthy();
    });

    it("renders all stat labels", () => {
        render(TelemetryHUD, {
            geom: defaultGeom,
            running: false,
            gen: 0,
            step: 0,
            stepsPerGen: 300,
            pop: 2048,
        });
        expect(screen.getByText("gen")).toBeTruthy();
        expect(screen.getByText("step")).toBeTruthy();
        expect(screen.getByText("pop")).toBeTruthy();
        expect(screen.getByText("fps")).toBeTruthy();
    });

    it("shows zero-padded gen and step values", () => {
        render(TelemetryHUD, {
            geom: defaultGeom,
            running: false,
            gen: 7,
            step: 42,
            stepsPerGen: 300,
            pop: 2048,
        });
        expect(screen.getByText("007")).toBeTruthy();
        expect(screen.getByText("042")).toBeTruthy();
    });

    it("shows -- for fps when not running", () => {
        render(TelemetryHUD, {
            geom: defaultGeom,
            running: false,
            gen: 0,
            step: 0,
            stepsPerGen: 300,
            pop: 0,
        });
        expect(screen.getByText("--")).toBeTruthy();
    });

    it("shows 60 for fps when running", () => {
        render(TelemetryHUD, {
            geom: defaultGeom,
            running: true,
            gen: 0,
            step: 0,
            stepsPerGen: 300,
            pop: 0,
        });
        expect(screen.getByText("60")).toBeTruthy();
    });
});
