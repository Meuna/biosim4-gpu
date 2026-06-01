import { render, screen } from "@testing-library/svelte";
import TopBar from "./TopBar.svelte";

const noOp = () => {};

describe("TopBar", () => {
    it("renders the brand name and subtitle", () => {
        render(TopBar, {
            running: false,
            onToggle: noOp,
            onStep: noOp,
            onGen: noOp,
            onReset: noOp,
        });
        expect(screen.getByText("biosim4-gpu")).toBeTruthy();
        expect(screen.getByText("visualizer · v0.1")).toBeTruthy();
    });

    it("renders the GitHub link", () => {
        render(TopBar, {
            running: false,
            onToggle: noOp,
            onStep: noOp,
            onGen: noOp,
            onReset: noOp,
        });
        const link = screen.getByRole("link", { name: /github/i });
        expect(link).toBeTruthy();
        expect((link as HTMLAnchorElement).href).toContain("github.com");
    });

    it("renders Play button inside the topbar when not running", () => {
        render(TopBar, {
            running: false,
            onToggle: noOp,
            onStep: noOp,
            onGen: noOp,
            onReset: noOp,
        });
        expect(
            screen.getByRole("button", { name: /play simulation/i }),
        ).toBeTruthy();
    });

    it("renders Stop button inside the topbar when running", () => {
        render(TopBar, {
            running: true,
            onToggle: noOp,
            onStep: noOp,
            onGen: noOp,
            onReset: noOp,
        });
        expect(
            screen.getByRole("button", { name: /stop simulation/i }),
        ).toBeTruthy();
    });

    it("renders Step, Restart, Next Gen, and Clear Genom buttons", () => {
        render(TopBar, {
            running: false,
            onToggle: noOp,
            onStep: noOp,
            onGen: noOp,
            onReset: noOp,
        });
        expect(
            screen.getByRole("button", { name: /step one simulation tick/i }),
        ).toBeTruthy();
        expect(
            screen.getByRole("button", { name: /restart simulation/i }),
        ).toBeTruthy();
        expect(
            screen.getByRole("button", { name: /advance one generation/i }),
        ).toBeTruthy();
        expect(
            screen.getByRole("button", { name: /clear genome/i }),
        ).toBeTruthy();
    });
});
