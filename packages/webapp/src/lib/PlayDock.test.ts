import { render, screen, fireEvent } from "@testing-library/svelte";
import PlayDock from "./PlayDock.svelte";

const defaultProps = {
    running: false,
    onToggle: () => {},
    onStep: () => {},
    onGen: () => {},
    onRestart: () => {},
};

describe("PlayDock", () => {
    it("shows Play button when not running", () => {
        render(PlayDock, defaultProps);
        expect(
            screen.getByRole("button", { name: /play simulation/i }),
        ).toBeTruthy();
        expect(
            screen.queryByRole("button", { name: /stop simulation/i }),
        ).toBeNull();
    });

    it("shows Stop button when running", () => {
        render(PlayDock, { ...defaultProps, running: true });
        expect(
            screen.getByRole("button", { name: /stop simulation/i }),
        ).toBeTruthy();
        expect(
            screen.queryByRole("button", { name: /play simulation/i }),
        ).toBeNull();
    });

    it("renders Step, Restart, and Next Gen buttons", () => {
        render(PlayDock, defaultProps);
        expect(
            screen.getByRole("button", { name: /step one simulation tick/i }),
        ).toBeTruthy();
        expect(
            screen.getByRole("button", { name: /restart simulation/i }),
        ).toBeTruthy();
        expect(
            screen.getByRole("button", { name: /advance one generation/i }),
        ).toBeTruthy();
    });

    it("renders Clear Genom button as disabled", () => {
        render(PlayDock, defaultProps);
        const btn = screen.getByRole("button", {
            name: /clear genome/i,
        }) as HTMLButtonElement;
        expect(btn).toBeTruthy();
        expect(btn.disabled).toBe(true);
    });

    it("calls onToggle when primary button clicked", () => {
        let called = false;
        render(PlayDock, {
            ...defaultProps,
            onToggle: () => {
                called = true;
            },
        });
        fireEvent.click(
            screen.getByRole("button", { name: /play simulation/i }),
        );
        expect(called).toBe(true);
    });

    it("calls onStep when Step button clicked", () => {
        let called = false;
        render(PlayDock, {
            ...defaultProps,
            onStep: () => {
                called = true;
            },
        });
        fireEvent.click(
            screen.getByRole("button", { name: /step one simulation tick/i }),
        );
        expect(called).toBe(true);
    });

    it("calls onRestart when Restart button clicked", () => {
        let called = false;
        render(PlayDock, {
            ...defaultProps,
            onRestart: () => {
                called = true;
            },
        });
        fireEvent.click(
            screen.getByRole("button", { name: /restart simulation/i }),
        );
        expect(called).toBe(true);
    });
});
