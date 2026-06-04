import { render, screen, fireEvent } from "@testing-library/svelte";
import { vi } from "vitest";
import SimConfigPanel from "./SimConfigPanel.svelte";
import type { SimParams } from "../workers/sim.worker";

const DEFAULTS: SimParams = {
    population: 3000,
    gridSizeX: 128,
    gridSizeY: 128,
    stepsPerGen: 300,
    maxGenomeLen: 24,
    maxNeurons: 5,
    pointMutationRate: 0.001,
    sexualReproduction: false,
    chooseParentsByFitness: false,
    losRange: 16,
    sensorRadius: 2,
    enableKill: false,
    responsivenessCurveK: 2.0,
    challenge: { kind: "x_band", xMin: 0.5, xMax: 1.0, mirror: false },
    barriers: [],
};

function renderPanel(overrides?: Partial<Parameters<typeof render>[1]>) {
    return render(SimConfigPanel, {
        props: {
            draftConfig: { ...DEFAULTS },
            isDirty: false,
            onDraftChange: vi.fn(),
            onRevert: vi.fn(),
            ...overrides?.props,
        },
    });
}

describe("SimConfigPanel", () => {
    it("renders the Simulation heading and Configuration eyebrow", () => {
        renderPanel();
        expect(
            screen.getByRole("heading", { name: "Simulation" }),
        ).toBeTruthy();
        expect(screen.getByText("Configuration")).toBeTruthy();
    });

    it("shows section labels sim.h, genome.h, io.h directly (no global toggle)", () => {
        renderPanel();
        expect(screen.getByText("sim.h")).toBeTruthy();
        expect(screen.getByText("genome.h")).toBeTruthy();
        expect(screen.getByText("io.h")).toBeTruthy();
    });

    it("renders ParamSlider for Population", () => {
        renderPanel();
        expect(screen.getByLabelText("Population")).toBeTruthy();
    });

    it("renders ParamSlider for Mutation rate", () => {
        renderPanel();
        expect(screen.getByLabelText("Mutation rate")).toBeTruthy();
    });

    it("renders GridSizeControl with the Grid section label", () => {
        renderPanel();
        expect(screen.getByText("Grid")).toBeTruthy();
        expect(screen.getByText("grid.h")).toBeTruthy();
    });

    it("shows challenge section with kind dropdown", () => {
        renderPanel();
        expect(screen.getByText("Challenge")).toBeTruthy();
        expect(screen.getByLabelText("Challenge kind")).toBeTruthy();
    });

    it("shows barriers section with add button", () => {
        renderPanel();
        expect(screen.getByText("Barriers")).toBeTruthy();
        expect(screen.getByLabelText("Add barrier")).toBeTruthy();
    });

    it("does not render revert button when isDirty is false", () => {
        renderPanel({ props: { isDirty: false } });
        expect(
            screen.queryByRole("button", { name: "Revert all changes" }),
        ).toBeNull();
    });

    it("renders revert button when isDirty is true", () => {
        renderPanel({ props: { isDirty: true } });
        expect(
            screen.getByRole("button", { name: "Revert all changes" }),
        ).toBeTruthy();
    });

    it("calls onRevert when revert button is clicked", async () => {
        const onRevert = vi.fn();
        renderPanel({ props: { isDirty: true, onRevert } });
        await fireEvent.click(
            screen.getByRole("button", { name: "Revert all changes" }),
        );
        expect(onRevert).toHaveBeenCalledOnce();
    });

    it("calls onDraftChange with updated population when slider changes", async () => {
        const onDraftChange = vi.fn<[SimParams], void>();
        renderPanel({ props: { onDraftChange } });
        await fireEvent.input(screen.getByLabelText("Population"), {
            target: { value: "500" },
        });
        expect(onDraftChange).toHaveBeenCalledOnce();
        const updated = onDraftChange.mock.calls[0][0];
        expect(updated.population).toBe(500);
    });
});
