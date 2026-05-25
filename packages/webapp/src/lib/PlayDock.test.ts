import { render, screen, fireEvent } from "@testing-library/svelte";
import PlayDock from "./PlayDock.svelte";

describe("PlayDock", () => {
    it("shows Play button when not running", () => {
        render(PlayDock, {
            running: false,
            centerX: 400,
            onToggle: () => {},
            onStep: () => {},
            onGen: () => {},
            onReset: () => {},
        });
        const btn = screen.getByRole("button", { name: /play simulation/i });
        expect(btn).toBeTruthy();
        expect(screen.queryByRole("button", { name: /pause/i })).toBeNull();
    });

    it("shows Pause button when running", () => {
        render(PlayDock, {
            running: true,
            centerX: 400,
            onToggle: () => {},
            onStep: () => {},
            onGen: () => {},
            onReset: () => {},
        });
        const btn = screen.getByRole("button", { name: /pause simulation/i });
        expect(btn).toBeTruthy();
    });

    it("renders Step, Gen, and Reset buttons", () => {
        render(PlayDock, {
            running: false,
            centerX: 400,
            onToggle: () => {},
            onStep: () => {},
            onGen: () => {},
            onReset: () => {},
        });
        expect(screen.getByRole("button", { name: /step/i })).toBeTruthy();
        expect(
            screen.getByRole("button", { name: /generation/i }),
        ).toBeTruthy();
        expect(screen.getByRole("button", { name: /reset/i })).toBeTruthy();
    });

    it("calls onToggle when primary button clicked", () => {
        let called = false;
        render(PlayDock, {
            running: false,
            centerX: 400,
            onToggle: () => {
                called = true;
            },
            onStep: () => {},
            onGen: () => {},
            onReset: () => {},
        });
        fireEvent.click(screen.getByRole("button", { name: /play/i }));
        expect(called).toBe(true);
    });
});
