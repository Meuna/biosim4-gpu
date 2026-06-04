import { render, screen, fireEvent } from "@testing-library/svelte";
import PlayDock from "./PlayDock.svelte";

const defaultProps = {
    running: false,
    genComplete: false,
    genomIncompatible: false,
    onToggle: () => {},
    onStep: () => {},
    onGen: () => {},
    onRewind: () => {},
    onClearGenom: () => {},
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

    it("renders Step, Next Gen, and Rewind buttons", () => {
        render(PlayDock, defaultProps);
        expect(
            screen.getByRole("button", { name: /step one simulation tick/i }),
        ).toBeTruthy();
        expect(screen.getByRole("button", { name: /rewind/i })).toBeTruthy();
        expect(
            screen.getByRole("button", { name: /advance one generation/i }),
        ).toBeTruthy();
    });

    it("renders Clear Genom button as enabled", () => {
        render(PlayDock, defaultProps);
        const btn = screen.getByRole("button", {
            name: /clear genome/i,
        }) as HTMLButtonElement;
        expect(btn).toBeTruthy();
        expect(btn.disabled).toBe(false);
    });

    it("shows ConfirmInline after clicking Clear Genom", async () => {
        render(PlayDock, defaultProps);
        await fireEvent.click(
            screen.getByRole("button", { name: /clear genome/i }),
        );
        expect(screen.getByText("Clear")).toBeTruthy();
        expect(screen.getByText("Cancel")).toBeTruthy();
    });

    it("calls onClearGenom after confirming clear", async () => {
        let called = false;
        render(PlayDock, {
            ...defaultProps,
            onClearGenom: () => {
                called = true;
            },
        });
        await fireEvent.click(
            screen.getByRole("button", { name: /clear genome/i }),
        );
        await fireEvent.click(screen.getByText("Clear"));
        expect(called).toBe(true);
    });

    it("cancels clear without calling onClearGenom", async () => {
        let called = false;
        render(PlayDock, {
            ...defaultProps,
            onClearGenom: () => {
                called = true;
            },
        });
        await fireEvent.click(
            screen.getByRole("button", { name: /clear genome/i }),
        );
        await fireEvent.click(screen.getByText("Cancel"));
        expect(called).toBe(false);
        expect(
            screen.getByRole("button", { name: /clear genome/i }),
        ).toBeTruthy();
    });

    it("disables Play button when genomIncompatible and not running", () => {
        render(PlayDock, { ...defaultProps, genomIncompatible: true });
        const btn = screen.getByRole("button", {
            name: /play simulation/i,
        }) as HTMLButtonElement;
        expect(btn.disabled).toBe(true);
    });

    it("does not disable Play button when genomIncompatible but running", () => {
        render(PlayDock, {
            ...defaultProps,
            running: true,
            genomIncompatible: true,
        });
        const btn = screen.getByRole("button", {
            name: /stop simulation/i,
        }) as HTMLButtonElement;
        expect(btn.disabled).toBe(false);
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

    it("calls onRewind when Rewind button clicked", () => {
        let called = false;
        render(PlayDock, {
            ...defaultProps,
            onRewind: () => {
                called = true;
            },
        });
        fireEvent.click(screen.getByRole("button", { name: /rewind/i }));
        expect(called).toBe(true);
    });
});
