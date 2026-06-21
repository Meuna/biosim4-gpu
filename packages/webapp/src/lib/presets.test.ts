import { describe, it, expect, vi, afterEach } from "vitest";
import { readFile, readdir } from "node:fs/promises";
import { resolve } from "node:path";
import {
    DEFAULT_PRESET,
    loadPresets,
    parseSnapshotCaps,
    checkSnapshotCaps,
    type PresetManifestEntry,
    type SnapshotCaps,
} from "./presets";
import { tomlToSimParams } from "./tomlConfig";
import type { SimParams } from "../workers/sim.worker";

// Vitest runs with cwd at packages/webapp (see the test script invocation).
const PRESETS_DIR = resolve(process.cwd(), "public/presets");

// A minimal valid 32-byte snapshot header ("BSM4" + the cap fields).
function snapHeader(maxGenes: number, maxNeurons: number): Uint8Array {
    const bytes = new Uint8Array(32);
    bytes.set([0x42, 0x53, 0x4d, 0x34], 0);
    new DataView(bytes.buffer).setUint16(12, maxGenes, true);
    bytes[14] = maxNeurons;
    return bytes;
}

afterEach(() => {
    vi.unstubAllGlobals();
});

describe("DEFAULT_PRESET", () => {
    it("resolves a clone of DEFAULTS and has no snapshot", async () => {
        expect(DEFAULT_PRESET.hasSnapshot).toBe(false);
        const conf = await DEFAULT_PRESET.loadConf();
        conf.population = 999;
        // A second load is unaffected — it must be a distinct clone.
        expect((await DEFAULT_PRESET.loadConf()).population).not.toBe(999);
        await expect(DEFAULT_PRESET.loadSnapshot()).rejects.toThrow();
    });
});

describe("loadPresets", () => {
    it("prepends the default preset to the fetched manifest entries", async () => {
        const manifest: PresetManifestEntry[] = [
            { id: "a", name: "Alpha", conf: "a.toml" },
            { id: "b", name: "Beta", conf: "b.toml", snapshot: "b.snap" },
        ];
        vi.stubGlobal(
            "fetch",
            vi.fn(async () => new Response(JSON.stringify(manifest))),
        );

        const presets = await loadPresets();
        expect(presets.map((p) => p.id)).toEqual(["default", "a", "b"]);
        expect(presets[1].hasSnapshot).toBe(false);
        expect(presets[2].hasSnapshot).toBe(true);
    });

    it("rejects when the manifest fetch fails", async () => {
        vi.stubGlobal(
            "fetch",
            vi.fn(async () => new Response("nope", { status: 404 })),
        );
        await expect(loadPresets()).rejects.toThrow(/manifest/);
    });

    it("loadConf fetches and parses the preset's TOML", async () => {
        const manifest: PresetManifestEntry[] = [
            { id: "a", name: "Alpha", conf: "a.toml" },
        ];
        const toml = '[challenge]\nkind = "corners"\nradius = 0.2\n';
        vi.stubGlobal(
            "fetch",
            vi.fn(async (url: string) =>
                url.endsWith("presets.json")
                    ? new Response(JSON.stringify(manifest))
                    : new Response(toml),
            ),
        );

        const [, alpha] = await loadPresets();
        const conf = await alpha.loadConf();
        expect(conf.challenge.kind).toBe("corners");
    });
});

describe("parseSnapshotCaps", () => {
    it("reads max-genes and max-neurons from the header", () => {
        expect(parseSnapshotCaps(snapHeader(24, 5))).toEqual({
            maxGenes: 24,
            maxNeurons: 5,
        });
    });

    it("throws on a short buffer", () => {
        expect(() => parseSnapshotCaps(new Uint8Array(10))).toThrow(/short/);
    });

    it("throws on a bad magic", () => {
        const bad = snapHeader(24, 5);
        bad[0] = 0;
        expect(() => parseSnapshotCaps(bad)).toThrow(/magic/);
    });
});

describe("checkSnapshotCaps", () => {
    const conf = (g: number, n: number) =>
        ({ maxGenes: g, maxNeurons: n }) as SimParams;
    const caps: SnapshotCaps = { maxGenes: 24, maxNeurons: 5 };

    it("passes when max-genes >= and max-neurons exactly equal", () => {
        expect(() => checkSnapshotCaps(conf(24, 5), caps)).not.toThrow();
        expect(() => checkSnapshotCaps(conf(32, 5), caps)).not.toThrow();
    });

    it("throws when max-genes is too low", () => {
        expect(() => checkSnapshotCaps(conf(16, 5), caps)).toThrow(/max-genes/);
    });

    it("throws when max-neurons differs (either direction)", () => {
        expect(() => checkSnapshotCaps(conf(24, 4), caps)).toThrow(
            /max-neurons/,
        );
        expect(() => checkSnapshotCaps(conf(24, 6), caps)).toThrow(
            /max-neurons/,
        );
    });
});

// Validates the *shipped* manifest against the on-disk assets: every conf parses,
// every referenced file exists, and any snapshot preset's caps line up. Guards a
// bad publish. Reads the real public/presets/ tree (not mocked).
describe("shipped presets manifest", () => {
    it("references existing, valid assets with matching snapshot caps", async () => {
        const manifest = JSON.parse(
            await readFile(`${PRESETS_DIR}/presets.json`, "utf8"),
        ) as PresetManifestEntry[];
        const files = new Set(await readdir(PRESETS_DIR));

        for (const entry of manifest) {
            expect(files.has(entry.conf)).toBe(true);
            const conf = tomlToSimParams(
                await readFile(`${PRESETS_DIR}/${entry.conf}`, "utf8"),
            );
            if (entry.snapshot !== undefined) {
                expect(files.has(entry.snapshot)).toBe(true);
                const bytes = new Uint8Array(
                    await readFile(`${PRESETS_DIR}/${entry.snapshot}`),
                );
                expect(() =>
                    checkSnapshotCaps(conf, parseSnapshotCaps(bytes)),
                ).not.toThrow();
            }
        }
    });
});
