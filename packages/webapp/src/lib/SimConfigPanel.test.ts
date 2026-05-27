import { render, screen, fireEvent } from "@testing-library/svelte";
import { vi } from "vitest";
import SimConfigPanel from "./SimConfigPanel.svelte";
import type { WorkerCmd } from "../workers/sim.worker";

describe("SimConfigPanel", () => {
    it("renders the Simulation heading and Configuration eyebrow", () => {
        render(SimConfigPanel, { props: { send: vi.fn() } });
        expect(
            screen.getByRole("heading", { name: "Simulation" }),
        ).toBeTruthy();
        expect(screen.getByText("Configuration")).toBeTruthy();
    });

    it("shows section labels sim.h, genome.h, io.h directly (no global toggle)", () => {
        render(SimConfigPanel, { props: { send: vi.fn() } });
        expect(screen.getByText("sim.h")).toBeTruthy();
        expect(screen.getByText("genome.h")).toBeTruthy();
        expect(screen.getByText("io.h")).toBeTruthy();
    });

    it("renders ParamSlider for Population", () => {
        render(SimConfigPanel, { props: { send: vi.fn() } });
        expect(screen.getByLabelText("Population")).toBeTruthy();
    });

    it("renders ParamSlider for Mutation rate", () => {
        render(SimConfigPanel, { props: { send: vi.fn() } });
        expect(screen.getByLabelText("Mutation rate")).toBeTruthy();
    });

    it("renders GridSizeControl with the Grid section label", () => {
        render(SimConfigPanel, { props: { send: vi.fn() } });
        expect(screen.getByText("Grid")).toBeTruthy();
        expect(screen.getByText("grid.h")).toBeTruthy();
    });

    it("shows challenge and barriers placeholder sections", () => {
        render(SimConfigPanel, { props: { send: vi.fn() } });
        expect(screen.getByText("Challenge")).toBeTruthy();
        expect(screen.getByText("Barriers")).toBeTruthy();
        const notes = screen.getAllByText(
            "Composite knobs not yet implemented.",
        );
        expect(notes.length).toBe(2);
    });

    it('starts in sync and shows "✓ in sync"', () => {
        render(SimConfigPanel, { props: { send: vi.fn() } });
        expect(screen.getByText("✓ in sync")).toBeTruthy();
    });

    it("marks apply button as dirty after changing a parameter", async () => {
        render(SimConfigPanel, { props: { send: vi.fn() } });
        await fireEvent.input(screen.getByLabelText("Population"), {
            target: { value: "500" },
        });
        expect(screen.getByText("apply & restart →")).toBeTruthy();
    });

    it("calls send with configure command when apply is clicked", async () => {
        const send = vi.fn<[WorkerCmd], void>();
        render(SimConfigPanel, { props: { send } });
        await fireEvent.input(screen.getByLabelText("Population"), {
            target: { value: "500" },
        });
        await fireEvent.click(
            screen.getByRole("button", {
                name: "Apply configuration and restart simulation",
            }),
        );
        expect(send).toHaveBeenCalledOnce();
        const cmd = send.mock.calls[0][0];
        expect(cmd.type).toBe("configure");
        if (cmd.type === "configure") {
            expect(cmd.params.population).toBe(500);
        }
    });

    it("resets dirty flag after apply", async () => {
        const send = vi.fn();
        render(SimConfigPanel, { props: { send } });
        await fireEvent.input(screen.getByLabelText("Population"), {
            target: { value: "500" },
        });
        await fireEvent.click(
            screen.getByRole("button", {
                name: "Apply configuration and restart simulation",
            }),
        );
        expect(screen.getByText("✓ in sync")).toBeTruthy();
    });
});
