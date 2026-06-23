import { render, screen } from "@testing-library/svelte";
import TelemetryHUD from "./TelemetryHUD.svelte";

const defaultGeom = { x: 80, y: 136, w: 600, h: 600, cx: 380, cy: 436 };

describe("TelemetryHUD", () => {
    it("renders the Simulation telemetry aria-label", () => {
        render(TelemetryHUD, {
            geom: defaultGeom,
            phase: "STEPS_PAUSED" as const,
            step: 0,
            stepsPerGen: 300,
            kills: 0,
        });
        expect(screen.getByLabelText("Simulation telemetry")).toBeTruthy();
    });

    it("renders all stat labels", () => {
        render(TelemetryHUD, {
            geom: defaultGeom,
            phase: "STEPS_PAUSED" as const,
            step: 0,
            stepsPerGen: 300,
            kills: 2048,
        });
        expect(screen.getByText("step")).toBeTruthy();
        expect(screen.getByText("kills")).toBeTruthy();
        expect(screen.getByText("fps")).toBeTruthy();
    });

    it("shows the zero-padded step value", () => {
        render(TelemetryHUD, {
            geom: defaultGeom,
            phase: "STEPS_PAUSED" as const,
            step: 42,
            stepsPerGen: 300,
            kills: 2048,
        });
        expect(screen.getByText("042")).toBeTruthy();
    });

    it("shows the kill count", () => {
        render(TelemetryHUD, {
            geom: defaultGeom,
            phase: "STEPS_PAUSED" as const,
            step: 0,
            stepsPerGen: 300,
            kills: 777,
        });
        expect(screen.getByText("777")).toBeTruthy();
    });

    it("shows -- for fps when not running", () => {
        render(TelemetryHUD, {
            geom: defaultGeom,
            phase: "STEPS_PAUSED" as const,
            step: 0,
            stepsPerGen: 300,
            kills: 0,
        });
        expect(screen.getByText("--")).toBeTruthy();
    });

    it("shows measured fps value when provided and running", () => {
        render(TelemetryHUD, {
            geom: defaultGeom,
            phase: "STEPS_RUNNING" as const,
            step: 0,
            stepsPerGen: 300,
            kills: 0,
            fps: 30,
        });
        expect(screen.getByText("30")).toBeTruthy();
    });

    it("positions right of the grid by default", () => {
        const { container } = render(TelemetryHUD, {
            geom: defaultGeom,
            phase: "STEPS_PAUSED" as const,
            step: 0,
            stepsPerGen: 300,
            kills: 0,
        });
        const aside = container.querySelector(".telemetry") as HTMLElement;
        expect(aside.classList.contains("telemetry--below")).toBe(false);
        // right of grid: left = x + w + 12 = 692, top = y = 136
        expect(aside.style.left).toBe("692px");
        expect(aside.style.top).toBe("136px");
    });

    it("positions below the grid when placement is below", () => {
        const { container } = render(TelemetryHUD, {
            geom: defaultGeom,
            placement: "below" as const,
            phase: "STEPS_PAUSED" as const,
            step: 0,
            stepsPerGen: 300,
            kills: 0,
        });
        const aside = container.querySelector(".telemetry") as HTMLElement;
        expect(aside.classList.contains("telemetry--below")).toBe(true);
        // below grid: top = y + h + 12 = 748, left = x = 80
        expect(aside.style.top).toBe("748px");
        expect(aside.style.left).toBe("80px");
    });
});
