import { render, screen, fireEvent } from "@testing-library/svelte";
import PlayDock from "./PlayDock.svelte";

const defaultProps = {
    phase: "WORKER_READY" as const,
    genomIncompatible: false,
    targetSpeed: 0,
    onToggle: () => {},
    onStep: () => {},
    onNextGen: () => {},
    onRewind: () => {},
    onClearGenom: () => {},
    onSetSpeed: () => {},
    onToggleFreeRun: () => {},
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
        render(PlayDock, {
            ...defaultProps,
            phase: "STEPS_RUNNING" as const,
        });
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
            phase: "STEPS_RUNNING" as const,
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

    it("renders Evolve button", () => {
        render(PlayDock, defaultProps);
        expect(screen.getByRole("button", { name: /evolve/i })).toBeTruthy();
    });

    it("Evolve button shows idle aria-label when not freeRunning", () => {
        render(PlayDock, defaultProps);
        const btn = screen.getByRole("button", {
            name: /evolve: auto-advance generations/i,
        }) as HTMLButtonElement;
        expect(btn).toBeTruthy();
        expect(btn.disabled).toBe(false);
    });

    it("Evolve button shows active aria-label when freeRunning", () => {
        render(PlayDock, {
            ...defaultProps,
            phase: "FREE_RUNNING" as const,
        });
        expect(
            screen.getByRole("button", { name: /stop evolving/i }),
        ).toBeTruthy();
    });

    it("disables Evolve button when running", () => {
        render(PlayDock, {
            ...defaultProps,
            phase: "STEPS_RUNNING" as const,
        });
        const btn = screen.getByRole("button", {
            name: /evolve/i,
        }) as HTMLButtonElement;
        expect(btn.disabled).toBe(true);
    });

    it("calls onToggleFreeRun when Evolve button clicked", () => {
        let called = false;
        render(PlayDock, {
            ...defaultProps,
            onToggleFreeRun: () => {
                called = true;
            },
        });
        fireEvent.click(screen.getByRole("button", { name: /evolve/i }));
        expect(called).toBe(true);
    });

    it("disables Evolve button when freeRunStopping", () => {
        render(PlayDock, {
            ...defaultProps,
            phase: "FREE_RUN_STOPPING" as const,
        });
        const btn = screen.getByRole("button", {
            name: /stop evolving/i,
        }) as HTMLButtonElement;
        expect(btn.disabled).toBe(true);
    });

    it("disables Step, Next Gen, and Rewind when freeRunning", () => {
        render(PlayDock, {
            ...defaultProps,
            phase: "FREE_RUNNING" as const,
        });
        const step = screen.getByRole("button", {
            name: /step one simulation tick/i,
        }) as HTMLButtonElement;
        const nextGen = screen.getByRole("button", {
            name: /advance one generation/i,
        }) as HTMLButtonElement;
        const rewind = screen.getByRole("button", {
            name: /rewind/i,
        }) as HTMLButtonElement;
        expect(step.disabled).toBe(true);
        expect(nextGen.disabled).toBe(true);
        expect(rewind.disabled).toBe(true);
    });

    it("disables Play when generation complete", () => {
        render(PlayDock, {
            ...defaultProps,
            phase: "GENERATION_ENDED" as const,
        });
        const btn = screen.getByRole("button", {
            name: /play simulation/i,
        }) as HTMLButtonElement;
        expect(btn.disabled).toBe(true);
    });

    it("shows the spent Ended label when generation complete", () => {
        render(PlayDock, {
            ...defaultProps,
            phase: "GENERATION_ENDED" as const,
        });
        const btn = screen.getByRole("button", {
            name: /play simulation/i,
        }) as HTMLButtonElement;
        expect(btn.textContent).toContain("End");
        expect(btn.classList.contains("dock__btn--spent")).toBe(true);
    });

    it("disables Step when genomIncompatible", () => {
        render(PlayDock, { ...defaultProps, genomIncompatible: true });
        const btn = screen.getByRole("button", {
            name: /step one simulation tick/i,
        }) as HTMLButtonElement;
        expect(btn.disabled).toBe(true);
    });

    it("disables Evolve when genomIncompatible", () => {
        render(PlayDock, { ...defaultProps, genomIncompatible: true });
        const btn = screen.getByRole("button", {
            name: /evolve/i,
        }) as HTMLButtonElement;
        expect(btn.disabled).toBe(true);
    });
});
