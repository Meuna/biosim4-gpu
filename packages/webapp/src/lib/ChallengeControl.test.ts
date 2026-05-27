import { render, screen, fireEvent } from "@testing-library/svelte";
import { vi } from "vitest";
import ChallengeControl from "./ChallengeControl.svelte";
import type { ChallengeSpec } from "../workers/sim.worker";

const defaultXBand: ChallengeSpec = {
    kind: "x_band",
    xMin: 0.5,
    xMax: 1.0,
    mirror: false,
};

describe("ChallengeControl", () => {
    it("renders the kind dropdown", () => {
        render(ChallengeControl, {
            props: { value: defaultXBand, onchange: vi.fn() },
        });
        expect(screen.getByLabelText("Challenge kind")).toBeTruthy();
    });

    it("shows x_band as the selected kind by default", () => {
        render(ChallengeControl, {
            props: { value: defaultXBand, onchange: vi.fn() },
        });
        const select = screen.getByLabelText(
            "Challenge kind",
        ) as HTMLSelectElement;
        expect(select.value).toBe("x_band");
    });

    it("shows X min and X max sliders for x_band", () => {
        render(ChallengeControl, {
            props: { value: defaultXBand, onchange: vi.fn() },
        });
        expect(screen.getByLabelText("X min")).toBeTruthy();
        expect(screen.getByLabelText("X max")).toBeTruthy();
    });

    it("shows disc sliders when kind is disc", () => {
        render(ChallengeControl, {
            props: {
                value: {
                    kind: "disc",
                    x: 0.5,
                    y: 0.5,
                    radius: 0.25,
                    weighted: false,
                },
                onchange: vi.fn(),
            },
        });
        expect(screen.getByLabelText("X")).toBeTruthy();
        expect(screen.getByLabelText("Y")).toBeTruthy();
        expect(screen.getByLabelText("Radius")).toBeTruthy();
    });

    it("hides param sliders for against_wall", () => {
        render(ChallengeControl, {
            props: { value: { kind: "against_wall" }, onchange: vi.fn() },
        });
        expect(screen.queryByLabelText("X min")).toBeNull();
        expect(screen.queryByLabelText("Radius")).toBeNull();
    });

    it("calls onchange with disc defaults when kind changes to disc", async () => {
        const onchange = vi.fn<[ChallengeSpec], void>();
        render(ChallengeControl, {
            props: { value: defaultXBand, onchange },
        });
        await fireEvent.change(screen.getByLabelText("Challenge kind"), {
            target: { value: "disc" },
        });
        expect(onchange).toHaveBeenCalledOnce();
        const spec = onchange.mock.calls[0][0];
        expect(spec.kind).toBe("disc");
    });

    it("calls onchange when X min slider changes", async () => {
        const onchange = vi.fn<[ChallengeSpec], void>();
        render(ChallengeControl, {
            props: { value: defaultXBand, onchange },
        });
        await fireEvent.input(screen.getByLabelText("X min"), {
            target: { value: "0.3" },
        });
        expect(onchange).toHaveBeenCalledOnce();
        const spec = onchange.mock.calls[0][0];
        if (spec.kind === "x_band") {
            expect(spec.xMin).toBeCloseTo(0.3);
        }
    });

    it("calls onchange when mirror radio is toggled on", async () => {
        const onchange = vi.fn<[ChallengeSpec], void>();
        render(ChallengeControl, {
            props: { value: defaultXBand, onchange },
        });
        await fireEvent.change(screen.getByRole("radio", { name: "Mirror" }), {
            target: { checked: true },
        });
        expect(onchange).toHaveBeenCalledOnce();
        const spec = onchange.mock.calls[0][0];
        if (spec.kind === "x_band") {
            expect(spec.mirror).toBe(true);
        }
    });
});
