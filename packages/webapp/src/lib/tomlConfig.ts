import { parse } from "smol-toml";
import type {
    SimParams,
    ChallengeSpec,
    BarrierSpec,
    BarrierKind,
    CornerQuadrant,
} from "../workers/sim.worker";

// Must match bindings.c s_params_mut and the C defaults in cfgparse.
export const DEFAULTS: SimParams = {
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

// ── Helpers ───────────────────────────────────────────────────────────────────

function numOf(v: unknown, fallback: number): number {
    return typeof v === "number" ? v : fallback;
}

function boolOf(v: unknown, fallback: boolean): boolean {
    return typeof v === "boolean" ? v : fallback;
}

function strOf(v: unknown, fallback: string): string {
    return typeof v === "string" ? v : fallback;
}

function tableOf(v: unknown): Record<string, unknown> {
    if (
        typeof v === "object" &&
        v !== null &&
        !Array.isArray(v) &&
        !(v instanceof Date)
    ) {
        return v as Record<string, unknown>;
    }
    return {};
}

// Emit a JS number as a TOML float literal (always includes a decimal point).
function tomlFloat(n: number): string {
    return Number.isInteger(n) ? `${n}.0` : String(n);
}

// ── Serializer ────────────────────────────────────────────────────────────────

function serializeChallenge(c: ChallengeSpec): string[] {
    const lines: string[] = [`kind = "${c.kind}"`];
    switch (c.kind) {
        case "x_band":
            lines.push(
                `x-min  = ${tomlFloat(c.xMin)}`,
                `x-max  = ${tomlFloat(c.xMax)}`,
                `mirror = ${c.mirror}`,
            );
            break;
        case "disc":
            lines.push(
                `x        = ${tomlFloat(c.x)}`,
                `y        = ${tomlFloat(c.y)}`,
                `radius   = ${tomlFloat(c.radius)}`,
                `weighted = ${c.weighted}`,
            );
            break;
        case "corners":
            lines.push(
                `radius   = ${tomlFloat(c.radius)}`,
                `weighted = ${c.weighted}`,
            );
            break;
        case "neighbor_count":
            lines.push(
                `radius         = ${tomlFloat(c.radius)}`,
                `min-n          = ${c.minN}`,
                `max-n          = ${c.maxN}`,
                `exclude-border = ${c.excludeBorder}`,
            );
            break;
        case "center_sparse":
            lines.push(
                `x        = ${tomlFloat(c.x)}`,
                `y        = ${tomlFloat(c.y)}`,
                `outer-r  = ${tomlFloat(c.outerR)}`,
                `inner-r  = ${tomlFloat(c.innerR)}`,
                `min-n    = ${c.minN}`,
                `max-n    = ${c.maxN}`,
                `weighted = ${c.weighted}`,
            );
            break;
        case "location_sequence":
        case "near_barrier":
            lines.push(`radius = ${tomlFloat(c.radius)}`);
            break;
        default:
            // Parameterless challenges: against_wall, migrate_distance,
            // touch_any_wall, radioactive_walls, pairs, altruism.
            break;
    }
    return lines;
}

function serializeBarrier(b: BarrierSpec, index: number): string[] {
    // The C core stores grid ratios in [0, 1] directly, so fractions are
    // written verbatim and scale with whatever grid size loads them.
    const lines: string[] = [`[barrier-${index}]`, `kind = "${b.kind}"`];
    if (b.x !== null) {
        lines.push(`x = ${tomlFloat(b.x)}`);
    }
    if (b.y !== null) {
        lines.push(`y = ${tomlFloat(b.y)}`);
    }
    // cfgparse uses "radius" as the length key for circle barriers.
    const lengthKey = b.kind === "circle" ? "radius" : "length";
    if (b.length !== null) {
        lines.push(`${lengthKey} = ${tomlFloat(b.length)}`);
    }
    if (b.kind !== "circle" && b.width !== null) {
        lines.push(`width = ${tomlFloat(b.width)}`);
    }
    if (b.kind === "corner") {
        lines.push(`quadrant = "${b.quadrant}"`);
    }
    return lines;
}

/** Serialise a SimParams object to a TOML string compatible with cfgparse. */
export function simParamsToToml(params: SimParams): string {
    const sections: string[] = [];

    sections.push(
        "[simulation]",
        `population    = ${params.population}`,
        `grid-size-x   = ${params.gridSizeX}`,
        `grid-size-y   = ${params.gridSizeY}`,
        `steps-per-gen = ${params.stepsPerGen}`,
    );

    sections.push(
        "",
        "[genome]",
        `max-genes            = ${params.maxGenes}`,
        `max-neurons               = ${params.maxNeurons}`,
        `point-mutation-rate       = ${tomlFloat(params.pointMutationRate)}`,
        `sexual-reproduction       = ${params.sexualReproduction}`,
        `choose-parents-by-fitness = ${params.chooseParentsByFitness}`,
    );

    sections.push(
        "",
        "[sensors]",
        `los-range     = ${params.losRange}`,
        `sensor-radius = ${params.sensorRadius}`,
    );

    sections.push(
        "",
        "[actions]",
        `enable-kill            = ${params.enableKill}`,
        `responsiveness-curve-k = ${tomlFloat(params.responsivenessCurveK)}`,
    );

    sections.push("", "[challenge]", ...serializeChallenge(params.challenge));

    sections.push("", "[barriers]", `num-barriers = ${params.barriers.length}`);

    for (let i = 0; i < params.barriers.length; i++) {
        sections.push("", ...serializeBarrier(params.barriers[i], i + 1));
    }

    return sections.join("\n") + "\n";
}

// ── Parser ────────────────────────────────────────────────────────────────────

const VALID_BARRIER_KINDS = new Set<string>([
    "hbar",
    "vbar",
    "square",
    "circle",
    "corner",
]);

const VALID_CORNER_QUADRANTS = new Set<string>(["ne", "nw", "se", "sw"]);

function parseBarrierKind(v: unknown): BarrierKind {
    return typeof v === "string" && VALID_BARRIER_KINDS.has(v)
        ? (v as BarrierKind)
        : "hbar";
}

function parseCornerQuadrant(v: unknown): CornerQuadrant {
    return typeof v === "string" && VALID_CORNER_QUADRANTS.has(v)
        ? (v as CornerQuadrant)
        : "ne";
}

function parseBarrier(table: Record<string, unknown>): BarrierSpec {
    // x/y/length/width are stored as grid ratios in [0, 1], read verbatim.
    const x = "x" in table ? numOf(table["x"], 0) : null;
    const y = "y" in table ? numOf(table["y"], 0) : null;

    // Accept both "length" and "radius" (circle alias) for the length field.
    const rawLength =
        "length" in table
            ? table["length"]
            : "radius" in table
              ? table["radius"]
              : undefined;
    const length = rawLength !== undefined ? numOf(rawLength, 0) : null;
    const width = "width" in table ? numOf(table["width"], 0) : null;

    return {
        kind: parseBarrierKind(table["kind"]),
        x,
        y,
        length,
        width,
        quadrant: parseCornerQuadrant(table["quadrant"]),
    };
}

function parseChallenge(
    table: Record<string, unknown>,
    fallback: ChallengeSpec,
): ChallengeSpec {
    const kind = strOf(table["kind"], fallback.kind);
    switch (kind) {
        case "x_band":
            return {
                kind,
                xMin: numOf(table["x-min"], 0.5),
                xMax: numOf(table["x-max"], 1.0),
                mirror: boolOf(table["mirror"], false),
            };
        case "disc":
            return {
                kind,
                x: numOf(table["x"], 0.5),
                y: numOf(table["y"], 0.5),
                radius: numOf(table["radius"], 0.333),
                weighted: boolOf(table["weighted"], false),
            };
        case "corners":
            return {
                kind,
                radius: numOf(table["radius"], 0.2),
                weighted: boolOf(table["weighted"], false),
            };
        case "neighbor_count":
            return {
                kind,
                radius: numOf(table["radius"], 0.1),
                minN: numOf(table["min-n"], 2),
                maxN: numOf(table["max-n"], 8),
                excludeBorder: boolOf(table["exclude-border"], false),
            };
        case "center_sparse":
            return {
                kind,
                x: numOf(table["x"], 0.5),
                y: numOf(table["y"], 0.5),
                outerR: numOf(table["outer-r"], 0.4),
                innerR: numOf(table["inner-r"], 0.2),
                minN: numOf(table["min-n"], 5),
                maxN: numOf(table["max-n"], 50),
                weighted: boolOf(table["weighted"], false),
            };
        case "location_sequence":
            return { kind, radius: numOf(table["radius"], 0.1) };
        case "near_barrier":
            return { kind, radius: numOf(table["radius"], 0.333) };
        case "against_wall":
        case "migrate_distance":
        case "touch_any_wall":
        case "radioactive_walls":
        case "pairs":
        case "altruism":
            return { kind };
        default:
            return fallback;
    }
}

/** Parse a TOML string into a SimParams, merging missing fields from DEFAULTS. */
export function tomlToSimParams(toml: string): SimParams {
    const doc = parse(toml) as Record<string, unknown>;

    const sim = tableOf(doc["simulation"]);
    const genome = tableOf(doc["genome"]);
    const sensors = tableOf(doc["sensors"]);
    const actions = tableOf(doc["actions"]);
    const challenge = tableOf(doc["challenge"]);
    const barriersSection = tableOf(doc["barriers"]);

    const gridSizeX = numOf(sim["grid-size-x"], DEFAULTS.gridSizeX);
    const gridSizeY = numOf(sim["grid-size-y"], DEFAULTS.gridSizeY);

    const numBarriers = numOf(barriersSection["num-barriers"], 0);
    const barriers: BarrierSpec[] = [];
    for (let i = 1; i <= numBarriers; i++) {
        const bt = tableOf(doc[`barrier-${i}`]);
        barriers.push(parseBarrier(bt));
    }

    return {
        population: numOf(sim["population"], DEFAULTS.population),
        gridSizeX,
        gridSizeY,
        stepsPerGen: numOf(sim["steps-per-gen"], DEFAULTS.stepsPerGen),
        maxGenes: numOf(genome["max-genes"], DEFAULTS.maxGenes),
        maxNeurons: numOf(genome["max-neurons"], DEFAULTS.maxNeurons),
        pointMutationRate: numOf(
            genome["point-mutation-rate"],
            DEFAULTS.pointMutationRate,
        ),
        sexualReproduction: boolOf(
            genome["sexual-reproduction"],
            DEFAULTS.sexualReproduction,
        ),
        chooseParentsByFitness: boolOf(
            genome["choose-parents-by-fitness"],
            DEFAULTS.chooseParentsByFitness,
        ),
        losRange: numOf(sensors["los-range"], DEFAULTS.losRange),
        sensorRadius: numOf(sensors["sensor-radius"], DEFAULTS.sensorRadius),
        enableKill: boolOf(actions["enable-kill"], DEFAULTS.enableKill),
        responsivenessCurveK: numOf(
            actions["responsiveness-curve-k"],
            DEFAULTS.responsivenessCurveK,
        ),
        challenge: parseChallenge(challenge, DEFAULTS.challenge),
        barriers,
    };
}
