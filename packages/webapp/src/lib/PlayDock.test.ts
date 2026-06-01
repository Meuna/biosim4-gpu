import { render, screen, fireEvent } from "@testing-library/svelte";
import PlayDock from "./PlayDock.svelte";

describe("PlayDock", () => {
    it("shows Play button when not running", () => {
        render(PlayDock, {
            running: false,
            onToggle: () => {},
            onStep: () => {},
            onGen: () => {},
            onReset: () => {},
        });
        expect(
            screen.getByRole("button", { name: /play simulation/i }),
        ).toBeTruthy();
        expect(
            screen.queryByRole("button", { name: /stop simulation/i }),
        ).toBeNull();
    });

    it("shows Stop button when running", () => {
        render(PlayDock, {
            running: true,
            onToggle: () => {},
            onStep: () => {},
            onGen: () => {},
            onReset: () => {},
        });
        expect(
            screen.getByRole("button", { name: /stop simulation/i }),
        ).toBeTruthy();
        expect(
            screen.queryByRole("button", { name: /play simulation/i }),
        ).toBeNull();
    });

    it("renders Step, Restart, and Next Gen buttons", () => {
        render(PlayDock, {
            running: false,
            onToggle: () => {},
            onStep: () => {},
            onGen: () => {},
            onReset: () => {},
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
    });

    it("renders Clear Genom button as disabled", () => {
        render(PlayDock, {
            running: false,
            onToggle: () => {},
            onStep: () => {},
            onGen: () => {},
            onReset: () => {},
        });
        const btn = screen.getByRole("button", {
            name: /clear genome/i,
        }) as HTMLButtonElement;
        expect(btn).toBeTruthy();
        expect(btn.disabled).toBe(true);
    });

    it("calls onToggle when primary button clicked", () => {
        let called = false;
        render(PlayDock, {
            running: false,
            onToggle: () => {
                called = true;
            },
            onStep: () => {},
            onGen: () => {},
            onReset: () => {},
        });
        fireEvent.click(
            screen.getByRole("button", { name: /play simulation/i }),
        );
        expect(called).toBe(true);
    });

    it("calls onStep when Step button clicked", () => {
        let called = false;
        render(PlayDock, {
            running: false,
            onToggle: () => {},
            onStep: () => {
                called = true;
            },
            onGen: () => {},
            onReset: () => {},
        });
        fireEvent.click(
            screen.getByRole("button", { name: /step one simulation tick/i }),
        );
        expect(called).toBe(true);
    });

    it("calls onReset when Restart button clicked", () => {
        let called = false;
        render(PlayDock, {
            running: false,
            onToggle: () => {},
            onStep: () => {},
            onGen: () => {},
            onReset: () => {
                called = true;
            },
        });
        fireEvent.click(
            screen.getByRole("button", { name: /restart simulation/i }),
        );
        expect(called).toBe(true);
    });
});
