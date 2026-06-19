import { render, screen, fireEvent } from "@testing-library/svelte";
import { vi } from "vitest";
import TopBar from "./TopBar.svelte";
import { webappVersion } from "./version";

const noOp = () => {};

const defaultProps = {
    phase: "WORKER_READY" as const,
    onToggle: noOp,
    onStep: noOp,
    onNextGen: noOp,
    onRewind: noOp,
    onClearGenom: noOp,
    onToggleFreeRun: noOp,
    targetSpeed: 0,
    onSetSpeed: noOp,
    onReturnToSculpture: noOp,
};

describe("TopBar", () => {
    it("renders the brand name and subtitle in the bar", () => {
        render(TopBar, defaultProps);
        expect(screen.getByText("biosim4-gpu")).toBeTruthy();
        expect(screen.getByText("visualizer")).toBeTruthy();
    });

    it("renders the build version below the bar", () => {
        render(TopBar, defaultProps);
        expect(screen.getByText(webappVersion)).toBeTruthy();
    });

    it("renders the GitHub link", () => {
        render(TopBar, defaultProps);
        const link = screen.getByRole("link", { name: /github/i });
        expect(link).toBeTruthy();
        expect((link as HTMLAnchorElement).href).toContain("github.com");
    });

    it("renders Play button inside the topbar when not running", () => {
        render(TopBar, defaultProps);
        expect(
            screen.getByRole("button", { name: /play simulation/i }),
        ).toBeTruthy();
    });

    it("renders Stop button inside the topbar when running", () => {
        render(TopBar, { ...defaultProps, phase: "STEPS_RUNNING" as const });
        expect(
            screen.getByRole("button", { name: /stop simulation/i }),
        ).toBeTruthy();
    });

    it("calls onReturnToSculpture when the brand is clicked while the sim is at rest", async () => {
        const onReturnToSculpture = vi.fn();
        render(TopBar, { ...defaultProps, onReturnToSculpture });
        const brand = screen.getByText("biosim4-gpu");
        await fireEvent.click(brand);
        expect(onReturnToSculpture).toHaveBeenCalledOnce();
    });

    it("does not call onReturnToSculpture when the brand is clicked while the sim is running", async () => {
        const onReturnToSculpture = vi.fn();
        render(TopBar, {
            ...defaultProps,
            phase: "STEPS_RUNNING" as const,
            onReturnToSculpture,
        });
        const brand = screen.getByText("biosim4-gpu");
        await fireEvent.click(brand);
        expect(onReturnToSculpture).not.toHaveBeenCalled();
    });

    it("renders Step, Rewind, Next Gen, and Clear Genom buttons", () => {
        render(TopBar, defaultProps);
        expect(
            screen.getByRole("button", { name: /step one simulation tick/i }),
        ).toBeTruthy();
        expect(screen.getByRole("button", { name: /rewind/i })).toBeTruthy();
        expect(
            screen.getByRole("button", { name: /advance one generation/i }),
        ).toBeTruthy();
        expect(
            screen.getByRole("button", { name: /clear genome/i }),
        ).toBeTruthy();
    });
});
