// Simulation presets — curated config + optional survivor snapshot bundles,
// shipped as static assets under public/presets/ and fetched at runtime. A
// preset is the "named preset" half of the concept (challenge / barriers /
// genome / sensors / actions); the orthogonal form-factor half (population +
// grid) lives in formFactor.ts. See docs/architecture.md.
import type { SimParams } from "../workers/sim.worker";
import { DEFAULTS, tomlToSimParams } from "./tomlConfig";

// One entry of the authored presets.json manifest.
export interface PresetManifestEntry {
    id: string;
    name: string;
    conf: string; // .toml filename relative to /presets/
    snapshot?: string; // optional .snap filename relative to /presets/
}

// A resolved preset. Conf and snapshot are loaded lazily on selection so the
// app only fetches the bytes the user actually picks.
export interface Preset {
    id: string;
    name: string;
    hasSnapshot: boolean;
    loadConf(): Promise<SimParams>;
    loadSnapshot(): Promise<Uint8Array>;
}

// Base path is BASE_URL-relative (never absolute "/presets/") so GitHub Pages
// deployments under a base path resolve correctly. Mirrors the wasm loader.
const PRESETS_BASE = `${import.meta.env.BASE_URL}presets/`;

// The built-in default preset — the in-code DEFAULTS, with no asset to fetch.
// Keeping it built-in lets startup seeding stay synchronous (the SimMachine is
// constructed before the manifest fetch resolves).
export const DEFAULT_PRESET: Preset = {
    id: "default",
    name: "Default",
    hasSnapshot: false,
    loadConf: () => Promise.resolve(structuredClone(DEFAULTS)),
    loadSnapshot: () =>
        Promise.reject(new Error("Default preset has no snapshot")),
};

async function fetchOk(url: string, what: string): Promise<Response> {
    const res = await fetch(url);
    if (!res.ok) {
        throw new Error(`Failed to fetch ${what} (${res.status})`);
    }
    return res;
}

function toPreset(entry: PresetManifestEntry): Preset {
    return {
        id: entry.id,
        name: entry.name,
        hasSnapshot: entry.snapshot !== undefined,
        loadConf: async () => {
            const res = await fetchOk(
                `${PRESETS_BASE}${entry.conf}`,
                `preset config ${entry.conf}`,
            );
            return tomlToSimParams(await res.text());
        },
        loadSnapshot: async () => {
            if (entry.snapshot === undefined) {
                throw new Error(`Preset ${entry.id} has no snapshot`);
            }
            const res = await fetchOk(
                `${PRESETS_BASE}${entry.snapshot}`,
                `preset snapshot ${entry.snapshot}`,
            );
            return new Uint8Array(await res.arrayBuffer());
        },
    };
}

// Fetches and resolves the preset list: the built-in default followed by every
// manifest entry. Rejects on a manifest fetch/parse failure so the caller can
// fall back to [DEFAULT_PRESET] and surface the error.
export async function loadPresets(): Promise<Preset[]> {
    const res = await fetchOk(`${PRESETS_BASE}presets.json`, "preset manifest");
    const entries = (await res.json()) as PresetManifestEntry[];
    return [DEFAULT_PRESET, ...entries.map(toPreset)];
}

// ── Snapshot cap validation ─────────────────────────────────────────────────

const SNAP_MAGIC = [0x42, 0x53, 0x4d, 0x34]; // "BSM4"

export interface SnapshotCaps {
    maxGenes: number;
    maxNeurons: number;
}

// Parse the genome caps from a snapshot's 32-byte file header (docs/formats.md):
// max_genes is a uint16 LE at offset 12, max_neurons a uint8 at offset 14.
// Throws on a bad magic so a corrupt / non-snapshot file is rejected.
export function parseSnapshotCaps(bytes: Uint8Array): SnapshotCaps {
    if (bytes.byteLength < 32) {
        throw new Error("Snapshot too short to contain a header");
    }
    if (SNAP_MAGIC.some((b, i) => bytes[i] !== b)) {
        throw new Error("Snapshot has an invalid magic header");
    }
    const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
    return {
        maxGenes: view.getUint16(12, true),
        maxNeurons: view.getUint8(14),
    };
}

// Assert a snapshot preset's conf caps line up with the snapshot's originating
// caps — the rule onSnapshotLoaded enforces: max-genes >= file (a pure slot
// count) and max-neurons EXACTLY equal (a larger or smaller cap silently
// rewires genomes via the % max_neurons remap). A mismatch ships a preset that
// gates play or corrupts behavior, so this is asserted in tests before publish.
export function checkSnapshotCaps(conf: SimParams, caps: SnapshotCaps): void {
    if (conf.maxGenes < caps.maxGenes) {
        throw new Error(
            `conf max-genes ${conf.maxGenes} < snapshot ${caps.maxGenes}`,
        );
    }
    if (conf.maxNeurons !== caps.maxNeurons) {
        throw new Error(
            `conf max-neurons ${conf.maxNeurons} != snapshot ${caps.maxNeurons}`,
        );
    }
}
