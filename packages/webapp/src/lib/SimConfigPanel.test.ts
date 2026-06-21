import { render, screen, fireEvent } from "@testing-library/svelte";
import { vi } from "vitest";
import SimConfigPanel from "./SimConfigPanel.svelte";
import type { SimParams } from "../workers/sim.worker";
import { DEFAULT_PRESET } from "./presets";

const DEFAULTS: SimParams = {
    population: 3000,
    gridSizeX: 128,
    gridSizeY: 128,
    stepsPerGen: 300,
    maxGenes: 24,
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
            presets: [DEFAULT_PRESET],
            selectedPresetId: DEFAULT_PRESET.id,
            onSelectPreset: vi.fn(),
            onDraftChange: vi.fn(),
            onConfUpload: vi.fn(),
            onConfDownload: vi.fn(),
            onConfCopy: vi.fn().mockResolvedValue(true),
            onSnapUpload: vi.fn(),
            onSnapDownload: vi.fn(),
            ...overrides?.props,
        },
    });
}

describe("SimConfigPanel", () => {
    it("renders the preset selector and Form factor pills", () => {
        renderPanel();
        expect(screen.getByText("Default")).toBeTruthy();
        expect(screen.getByText("Form factor")).toBeTruthy();
        expect(screen.getByRole("button", { name: "Desktop" })).toBeTruthy();
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

    it("shows barrier preset pills", () => {
        renderPanel();
        expect(screen.getByLabelText("Cross preset")).toBeTruthy();
        expect(screen.getByLabelText("Random preset")).toBeTruthy();
    });

    it("clicking a preset replaces the barriers array", async () => {
        const onDraftChange = vi.fn<[SimParams], void>();
        renderPanel({
            props: {
                draftConfig: {
                    ...DEFAULTS,
                    barriers: [
                        {
                            kind: "circle",
                            x: 0.1,
                            y: 0.1,
                            length: 0.1,
                            width: null,
                            quadrant: "ne",
                        },
                    ],
                },
                onDraftChange,
            },
        });
        await fireEvent.click(screen.getByLabelText("Cross preset"));
        expect(onDraftChange).toHaveBeenCalledOnce();
        const updated = onDraftChange.mock.calls[0][0];
        expect(updated.barriers).toHaveLength(2);
        expect(updated.barriers.map((b) => b.kind).sort()).toEqual([
            "hbar",
            "vbar",
        ]);
    });

    it("shows the 'Add a barrier here' warning for near_barrier with no barriers", () => {
        renderPanel({
            props: {
                draftConfig: {
                    ...DEFAULTS,
                    challenge: { kind: "near_barrier", radius: 1.5 },
                    barriers: [],
                },
            },
        });
        expect(screen.getByText(/add a barrier here/i)).toBeTruthy();
    });

    it("hides the 'Add a barrier here' warning once a barrier exists", () => {
        renderPanel({
            props: {
                draftConfig: {
                    ...DEFAULTS,
                    challenge: { kind: "near_barrier", radius: 1.5 },
                    barriers: [
                        {
                            kind: "hbar",
                            x: 0.5,
                            y: 0.5,
                            length: 0.25,
                            width: 0.02,
                            quadrant: "ne",
                        },
                    ],
                },
            },
        });
        expect(screen.queryByText(/add a barrier here/i)).toBeNull();
    });

    it("calls onDraftChange with updated population when slider changes", async () => {
        const onDraftChange = vi.fn<[SimParams], void>();
        renderPanel({ props: { onDraftChange } });
        await fireEvent.change(screen.getByLabelText("Population"), {
            target: { value: "500" },
        });
        expect(onDraftChange).toHaveBeenCalledOnce();
        const updated = onDraftChange.mock.calls[0][0];
        expect(updated.population).toBe(500);
    });

    it("calls onSnapUpload when the upload button is clicked", async () => {
        const onSnapUpload = vi.fn();
        renderPanel({ props: { onSnapUpload } });
        await fireEvent.click(screen.getByLabelText("Upload snapshot"));
        expect(onSnapUpload).toHaveBeenCalledOnce();
    });

    it("calls onConfCopy when the copy button is clicked", async () => {
        const onConfCopy = vi.fn().mockResolvedValue(true);
        renderPanel({ props: { onConfCopy } });
        await fireEvent.click(screen.getByLabelText("Copy config"));
        expect(onConfCopy).toHaveBeenCalledOnce();
    });

    it("flashes a checkmark after a successful copy", async () => {
        const { container } = renderPanel({
            props: { onConfCopy: vi.fn().mockResolvedValue(true) },
        });
        expect(container.querySelector(".lucide-check")).toBeNull();
        await fireEvent.click(screen.getByLabelText("Copy config"));
        expect(container.querySelector(".lucide-check")).not.toBeNull();
    });

    it("disables uploads and config controls when changeDisabled", () => {
        renderPanel({ props: { changeDisabled: true } });
        expect(
            (screen.getByLabelText("Upload config") as HTMLButtonElement)
                .disabled,
        ).toBe(true);
        expect(
            (screen.getByLabelText("Upload snapshot") as HTMLButtonElement)
                .disabled,
        ).toBe(true);
        expect(
            (screen.getByLabelText("Population") as HTMLInputElement).disabled,
        ).toBe(true);
    });
});
