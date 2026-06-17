import { DEFAULTS, simParamsToToml, tomlToSimParams } from "./tomlConfig";
import type { SimParams } from "../workers/sim.worker";

function roundTrip(params: SimParams): SimParams {
    return tomlToSimParams(simParamsToToml(params));
}

describe("simParamsToToml / tomlToSimParams", () => {
    it("round-trips DEFAULTS without loss", () => {
        const result = roundTrip(DEFAULTS);
        expect(result.population).toBe(DEFAULTS.population);
        expect(result.gridSizeX).toBe(DEFAULTS.gridSizeX);
        expect(result.gridSizeY).toBe(DEFAULTS.gridSizeY);
        expect(result.stepsPerGen).toBe(DEFAULTS.stepsPerGen);
        expect(result.maxGenes).toBe(DEFAULTS.maxGenes);
        expect(result.maxNeurons).toBe(DEFAULTS.maxNeurons);
        expect(result.pointMutationRate).toBeCloseTo(
            DEFAULTS.pointMutationRate,
            10,
        );
        expect(result.sexualReproduction).toBe(DEFAULTS.sexualReproduction);
        expect(result.chooseParentsByFitness).toBe(
            DEFAULTS.chooseParentsByFitness,
        );
        expect(result.losRange).toBe(DEFAULTS.losRange);
        expect(result.sensorRadius).toBe(DEFAULTS.sensorRadius);
        expect(result.enableKill).toBe(DEFAULTS.enableKill);
        expect(result.responsivenessCurveK).toBeCloseTo(
            DEFAULTS.responsivenessCurveK,
            10,
        );
        expect(result.barriers).toHaveLength(0);
    });

    describe("challenge round-trips", () => {
        it("x_band", () => {
            const p = {
                ...DEFAULTS,
                challenge: {
                    kind: "x_band" as const,
                    xMin: 0.25,
                    xMax: 0.75,
                    mirror: true,
                },
            };
            const r = roundTrip(p);
            expect(r.challenge).toEqual(p.challenge);
        });

        it("disc", () => {
            const p = {
                ...DEFAULTS,
                challenge: {
                    kind: "disc" as const,
                    x: 0.4,
                    y: 0.6,
                    radius: 0.2,
                    weighted: true,
                },
            };
            const r = roundTrip(p);
            expect(r.challenge).toEqual(p.challenge);
        });

        it("corners", () => {
            const p = {
                ...DEFAULTS,
                challenge: {
                    kind: "corners" as const,
                    radius: 0.15,
                    weighted: false,
                },
            };
            const r = roundTrip(p);
            expect(r.challenge).toEqual(p.challenge);
        });

        it("neighbor_count", () => {
            const p = {
                ...DEFAULTS,
                challenge: {
                    kind: "neighbor_count" as const,
                    radius: 0.1,
                    minN: 3,
                    maxN: 10,
                    excludeBorder: true,
                },
            };
            const r = roundTrip(p);
            expect(r.challenge).toEqual(p.challenge);
        });

        it("center_sparse", () => {
            const p = {
                ...DEFAULTS,
                challenge: {
                    kind: "center_sparse" as const,
                    x: 0.5,
                    y: 0.5,
                    outerR: 0.4,
                    innerR: 0.2,
                    minN: 5,
                    maxN: 50,
                    weighted: true,
                },
            };
            const r = roundTrip(p);
            expect(r.challenge).toEqual(p.challenge);
        });

        it("location_sequence", () => {
            const p = {
                ...DEFAULTS,
                challenge: { kind: "location_sequence" as const, radius: 0.1 },
            };
            const r = roundTrip(p);
            expect(r.challenge).toEqual(p.challenge);
        });

        it("near_barrier", () => {
            const p = {
                ...DEFAULTS,
                challenge: { kind: "near_barrier" as const, radius: 0.333 },
            };
            const r = roundTrip(p);
            expect(r.challenge).toEqual(p.challenge);
        });

        it.each([
            "against_wall",
            "migrate_distance",
            "touch_any_wall",
            "radioactive_walls",
            "pairs",
            "altruism",
        ] as const)("parameterless: %s", (kind) => {
            const p = { ...DEFAULTS, challenge: { kind } };
            const r = roundTrip(p);
            expect(r.challenge.kind).toBe(kind);
        });
    });

    describe("barrier round-trips", () => {
        it("barrier with all fields specified", () => {
            const p: SimParams = {
                ...DEFAULTS,
                gridSizeX: 64,
                gridSizeY: 64,
                barriers: [
                    {
                        kind: "hbar",
                        x: 0.5,
                        y: 0.25,
                        length: 0.5,
                        width: 0.03125,
                    },
                ],
            };
            const r = roundTrip(p);
            expect(r.barriers).toHaveLength(1);
            const b = r.barriers[0];
            expect(b.kind).toBe("hbar");
            // All fields are stored as grid ratios and round-trip exactly.
            expect(b.x).toBeCloseTo(0.5, 5);
            expect(b.y).toBeCloseTo(0.25, 5);
            expect(b.length).toBeCloseTo(0.5, 5);
            expect(b.width).toBeCloseTo(0.03125, 5);
        });

        it("barrier with null fields (random placement)", () => {
            const p: SimParams = {
                ...DEFAULTS,
                barriers: [
                    {
                        kind: "vbar",
                        x: null,
                        y: null,
                        length: null,
                        width: null,
                    },
                ],
            };
            const r = roundTrip(p);
            expect(r.barriers).toHaveLength(1);
            const b = r.barriers[0];
            expect(b.kind).toBe("vbar");
            expect(b.x).toBeNull();
            expect(b.y).toBeNull();
            expect(b.length).toBeNull();
            expect(b.width).toBeNull();
        });

        it("circle barrier uses radius key and ignores width", () => {
            const p: SimParams = {
                ...DEFAULTS,
                gridSizeX: 128,
                gridSizeY: 128,
                barriers: [
                    {
                        kind: "circle",
                        x: 0.5,
                        y: 0.5,
                        length: 0.1,
                        width: 0.05,
                    },
                ],
            };
            const toml = simParamsToToml(p);
            expect(toml).toContain("radius =");
            expect(toml).not.toContain("width =");
            const r = tomlToSimParams(toml);
            expect(r.barriers[0].length).toBeCloseTo(0.1, 5);
        });

        it("multiple barriers round-trip", () => {
            const p: SimParams = {
                ...DEFAULTS,
                gridSizeX: 64,
                gridSizeY: 64,
                barriers: [
                    {
                        kind: "hbar",
                        x: 0.5,
                        y: 0.25,
                        length: 0.5,
                        width: 0.03125,
                    },
                    {
                        kind: "vbar",
                        x: null,
                        y: 0.75,
                        length: 0.4,
                        width: null,
                    },
                ],
            };
            const r = roundTrip(p);
            expect(r.barriers).toHaveLength(2);
            expect(r.barriers[0].kind).toBe("hbar");
            expect(r.barriers[1].kind).toBe("vbar");
            expect(r.barriers[1].x).toBeNull();
            expect(r.barriers[1].width).toBeNull();
        });
    });

    it("unknown TOML keys are silently ignored", () => {
        const toml = `
[simulation]
population = 500
unknown-future-key = 42

[genome]
max-genes = 12

[sensors]
[actions]
[challenge]
kind = "against_wall"
[barriers]
num-barriers = 0
`;
        expect(() => tomlToSimParams(toml)).not.toThrow();
        const r = tomlToSimParams(toml);
        expect(r.population).toBe(500);
        expect(r.maxGenes).toBe(12);
    });

    it("missing sections fall back to DEFAULTS", () => {
        const toml = `
[simulation]
population = 200
[genome]
[sensors]
[actions]
[challenge]
kind = "x_band"
x-min = 0.5
x-max = 1.0
mirror = false
[barriers]
num-barriers = 0
`;
        const r = tomlToSimParams(toml);
        expect(r.population).toBe(200);
        expect(r.gridSizeX).toBe(DEFAULTS.gridSizeX);
        expect(r.stepsPerGen).toBe(DEFAULTS.stepsPerGen);
        expect(r.maxGenes).toBe(DEFAULTS.maxGenes);
    });

    it("malformed TOML throws", () => {
        expect(() => tomlToSimParams("not valid toml = = =")).toThrow();
    });

    it("emits valid TOML sections in the expected order", () => {
        const toml = simParamsToToml(DEFAULTS);
        const simIdx = toml.indexOf("[simulation]");
        const genomeIdx = toml.indexOf("[genome]");
        const sensorsIdx = toml.indexOf("[sensors]");
        const actionsIdx = toml.indexOf("[actions]");
        const challengeIdx = toml.indexOf("[challenge]");
        const barriersIdx = toml.indexOf("[barriers]");
        expect(simIdx).toBeGreaterThanOrEqual(0);
        expect(genomeIdx).toBeGreaterThan(simIdx);
        expect(sensorsIdx).toBeGreaterThan(genomeIdx);
        expect(actionsIdx).toBeGreaterThan(sensorsIdx);
        expect(challengeIdx).toBeGreaterThan(actionsIdx);
        expect(barriersIdx).toBeGreaterThan(challengeIdx);
    });
});
