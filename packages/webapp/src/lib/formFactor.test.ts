import {
    activeFormFactor,
    applyFormFactor,
    classifyFormFactor,
    configForFormFactor,
    detectFormFactor,
} from "./formFactor";
import { DEFAULTS } from "./tomlConfig";
import type { SimParams } from "../workers/sim.worker";

describe("classifyFormFactor", () => {
    it("is desktop whenever a hover-capable fine pointer is present", () => {
        // Even a narrow window: the pointer trait wins over viewport width, so
        // a touch-capable laptop is not misclassified as a handheld device.
        expect(
            classifyFormFactor({ hoverFinePointer: true, viewportW: 400 }),
        ).toBe("desktop");
        expect(
            classifyFormFactor({ hoverFinePointer: true, viewportW: 1440 }),
        ).toBe("desktop");
    });

    it("is phone for a handheld below the 760px split", () => {
        expect(
            classifyFormFactor({ hoverFinePointer: false, viewportW: 759 }),
        ).toBe("phone");
    });

    it("is tablet for a handheld at or above the 760px split", () => {
        expect(
            classifyFormFactor({ hoverFinePointer: false, viewportW: 760 }),
        ).toBe("tablet");
        expect(
            classifyFormFactor({ hoverFinePointer: false, viewportW: 1024 }),
        ).toBe("tablet");
    });
});

describe("configForFormFactor", () => {
    it("returns DEFAULTS unchanged for desktop", () => {
        expect(configForFormFactor("desktop")).toEqual(DEFAULTS);
    });

    it("overrides only population and grid for tablet", () => {
        expect(configForFormFactor("tablet")).toEqual({
            ...DEFAULTS,
            population: 1500,
            gridSizeX: 96,
            gridSizeY: 96,
        });
    });

    it("overrides only population and grid for phone", () => {
        expect(configForFormFactor("phone")).toEqual({
            ...DEFAULTS,
            population: 700,
            gridSizeX: 64,
            gridSizeY: 64,
        });
    });

    it("returns a distinct clone that does not alias DEFAULTS", () => {
        const cfg = configForFormFactor("phone");
        cfg.barriers.push({
            kind: "hbar",
            x: null,
            y: null,
            length: 0.5,
            width: null,
            quadrant: "ne",
        });
        cfg.challenge = { kind: "radioactive_walls" };
        expect(DEFAULTS.barriers).toEqual([]);
        expect(DEFAULTS.challenge.kind).toBe("x_band");
    });
});

describe("applyFormFactor", () => {
    it("overrides only pop/grid and keeps the rest of the base config", () => {
        const base = {
            ...DEFAULTS,
            challenge: { kind: "corners", radius: 0.2, weighted: true },
            maxNeurons: 9,
        } as SimParams;
        const out = applyFormFactor(base, "phone");
        expect(out).toEqual({
            ...base,
            population: 700,
            gridSizeX: 64,
            gridSizeY: 64,
        });
    });

    it("does not mutate the base config", () => {
        const base = structuredClone(DEFAULTS);
        applyFormFactor(base, "phone");
        expect(base.population).toBe(DEFAULTS.population);
        expect(base.gridSizeX).toBe(DEFAULTS.gridSizeX);
    });

    it("passes nested challenge/barriers through to the result", () => {
        const challenge = { kind: "corners", radius: 0.2, weighted: true };
        const base = { ...DEFAULTS, challenge } as SimParams;
        expect(applyFormFactor(base, "tablet").challenge).toBe(challenge);
    });
});

describe("activeFormFactor", () => {
    it("returns the matching form factor for desktop/tablet/phone pop+grid", () => {
        expect(activeFormFactor(configForFormFactor("desktop"))).toBe(
            "desktop",
        );
        expect(activeFormFactor(configForFormFactor("tablet"))).toBe("tablet");
        expect(activeFormFactor(configForFormFactor("phone"))).toBe("phone");
    });

    it("returns null when pop/grid do not match any form factor", () => {
        expect(
            activeFormFactor({ ...DEFAULTS, gridSizeX: 100, gridSizeY: 100 }),
        ).toBeNull();
        expect(activeFormFactor({ ...DEFAULTS, population: 1234 })).toBeNull();
    });
});

describe("detectFormFactor", () => {
    it("forwards the live media query and viewport to the classifier", () => {
        const matchMedia = vi.fn((query: string) => ({
            matches: query === "(hover: hover) and (pointer: fine)",
        }));
        vi.stubGlobal("matchMedia", matchMedia);
        vi.stubGlobal("innerWidth", 500);

        expect(detectFormFactor()).toBe("desktop");
        expect(matchMedia).toHaveBeenCalledWith(
            "(hover: hover) and (pointer: fine)",
        );

        vi.unstubAllGlobals();
    });
});
