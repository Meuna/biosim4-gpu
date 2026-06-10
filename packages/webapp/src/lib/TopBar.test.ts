import { render, screen } from "@testing-library/svelte";
import TopBar from "./TopBar.svelte";

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
};

describe("TopBar", () => {
    it("renders the brand name and subtitle", () => {
        render(TopBar, defaultProps);
        expect(screen.getByText("biosim4-gpu")).toBeTruthy();
        expect(screen.getByText("visualizer · v0.1")).toBeTruthy();
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
