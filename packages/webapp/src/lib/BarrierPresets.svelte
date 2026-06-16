<script lang="ts">
    // BarrierPresets — one-click pills that replace the barrier list with a
    // common layout. Kept separate from BarrierControl so the latter stays a
    // pure value/onchange control.
    import type { BarrierSpec } from "../workers/sim.worker";
    import { Dices } from "lucide-svelte";

    interface Props {
        disabled?: boolean;
        onapply: (barriers: BarrierSpec[]) => void;
    }
    const { disabled = false, onapply }: Props = $props();

    const BAR_WIDTH = 0.025;

    function bar(
        kind: "hbar" | "vbar",
        x: number,
        y: number,
        length: number,
    ): BarrierSpec {
        return { kind, x, y, length, width: BAR_WIDTH };
    }

    function circle(x: number, y: number, radius: number): BarrierSpec {
        return { kind: "circle", x, y, length: radius, width: null };
    }

    // One corner of the bar-cross / square layouts: an L made of one hbar and
    // one vbar. `hx`/`vy` place the arm centres; the shared vertex is (vx, hy).
    function cornerL(
        vx: number,
        hy: number,
        hx: number,
        vy: number,
        arm: number,
    ): BarrierSpec[] {
        return [bar("hbar", hx, hy, arm), bar("vbar", vx, vy, arm)];
    }

    function buildCross(): BarrierSpec[] {
        return [bar("hbar", 0.5, 0.5, 0.6), bar("vbar", 0.5, 0.5, 0.6)];
    }

    function buildVerticalSplit(): BarrierSpec[] {
        return [
            bar("vbar", 0.5, 0.5, 1.0),
            bar("vbar", 0.25, 0.25, 0.45),
            bar("vbar", 0.25, 0.75, 0.45),
            bar("vbar", 0.75, 0.25, 0.45),
            bar("vbar", 0.75, 0.75, 0.45),
        ];
    }

    // Brackets hug the outer corners, arms reaching toward the centre, so the
    // open channels meet in a plus shape.
    function buildBarCross(): BarrierSpec[] {
        const arm = 0.3;
        return [
            ...cornerL(0.2, 0.2, 0.35, 0.35, arm),
            ...cornerL(0.8, 0.2, 0.65, 0.35, arm),
            ...cornerL(0.2, 0.8, 0.35, 0.65, arm),
            ...cornerL(0.8, 0.8, 0.65, 0.65, arm),
        ];
    }

    // Inverted brackets: arms reach outward toward the corners, forming a box.
    function buildSquare(): BarrierSpec[] {
        const arm = 0.3;
        return [
            ...cornerL(0.35, 0.35, 0.2, 0.2, arm),
            ...cornerL(0.65, 0.35, 0.8, 0.2, arm),
            ...cornerL(0.35, 0.65, 0.2, 0.8, arm),
            ...cornerL(0.65, 0.65, 0.8, 0.8, arm),
        ];
    }

    function buildFiveDots(): BarrierSpec[] {
        const r = 0.08;
        return [
            circle(0.5, 0.5, r),
            circle(0.27, 0.27, r),
            circle(0.73, 0.27, r),
            circle(0.27, 0.73, r),
            circle(0.73, 0.73, r),
        ];
    }

    function buildRandom(): BarrierSpec[] {
        const count = 10 + Math.floor(Math.random() * 11); // 10..20
        const out: BarrierSpec[] = [];
        for (let i = 0; i < count; i++) {
            out.push(
                circle(
                    0.1 + Math.random() * 0.8,
                    0.1 + Math.random() * 0.8,
                    0.05,
                ),
            );
        }
        return out;
    }

    type PresetId =
        | "cross"
        | "vsplit"
        | "barcross"
        | "square"
        | "dots"
        | "random";

    const PRESETS: {
        id: PresetId;
        label: string;
        build: () => BarrierSpec[];
    }[] = [
        { id: "cross", label: "Cross", build: buildCross },
        { id: "vsplit", label: "Vertical split", build: buildVerticalSplit },
        { id: "barcross", label: "Bar cross", build: buildBarCross },
        { id: "square", label: "Square", build: buildSquare },
        { id: "dots", label: "5 dots", build: buildFiveDots },
        { id: "random", label: "Random", build: buildRandom },
    ];
</script>

<div class="barrier-presets__pills">
    {#each PRESETS as preset (preset.id)}
        <button
            class="button button--ghost button--pill barrier-presets__pill"
            {disabled}
            onclick={() => onapply(preset.build())}
            aria-label={`${preset.label} preset`}
        >
            <span class="barrier-presets__glyph" aria-hidden="true">
                {#if preset.id === "cross"}
                    <svg viewBox="0 0 24 24" fill="currentColor">
                        <rect x="4" y="11" width="16" height="2" rx="1" />
                        <rect x="11" y="4" width="2" height="16" rx="1" />
                    </svg>
                {:else if preset.id === "vsplit"}
                    <svg viewBox="0 0 24 24" fill="currentColor">
                        <rect x="11" y="3" width="2" height="18" rx="1" />
                        <rect x="5" y="3" width="2" height="7" rx="1" />
                        <rect x="5" y="14" width="2" height="7" rx="1" />
                        <rect x="17" y="3" width="2" height="7" rx="1" />
                        <rect x="17" y="14" width="2" height="7" rx="1" />
                    </svg>
                {:else if preset.id === "barcross"}
                    <svg viewBox="0 0 24 24" fill="currentColor">
                        <rect x="4" y="4" width="7" height="2" rx="1" />
                        <rect x="4" y="4" width="2" height="7" rx="1" />
                        <rect x="13" y="4" width="7" height="2" rx="1" />
                        <rect x="18" y="4" width="2" height="7" rx="1" />
                        <rect x="4" y="18" width="7" height="2" rx="1" />
                        <rect x="4" y="13" width="2" height="7" rx="1" />
                        <rect x="13" y="18" width="7" height="2" rx="1" />
                        <rect x="18" y="13" width="2" height="7" rx="1" />
                    </svg>
                {:else if preset.id === "square"}
                    <svg viewBox="0 0 24 24" fill="currentColor">
                        <rect x="8" y="8" width="6" height="2" rx="1" />
                        <rect x="8" y="8" width="2" height="6" rx="1" />
                        <rect x="14" y="8" width="2" height="2" rx="1" />
                        <rect x="14" y="8" width="2" height="6" rx="1" />
                        <rect x="8" y="14" width="6" height="2" rx="1" />
                        <rect x="8" y="14" width="2" height="2" rx="1" />
                        <rect x="14" y="14" width="2" height="2" rx="1" />
                    </svg>
                {:else if preset.id === "dots"}
                    <svg viewBox="0 0 24 24" fill="currentColor">
                        <circle cx="12" cy="12" r="2" />
                        <circle cx="6" cy="6" r="2" />
                        <circle cx="18" cy="6" r="2" />
                        <circle cx="6" cy="18" r="2" />
                        <circle cx="18" cy="18" r="2" />
                    </svg>
                {:else}
                    <Dices size={16} />
                {/if}
            </span>
            <span>{preset.label}</span>
        </button>
    {/each}
</div>

<style>
    .barrier-presets__pills {
        display: flex;
        flex-wrap: wrap;
        gap: var(--space-2);
        margin-bottom: var(--space-3);
    }

    .barrier-presets__pill {
        gap: var(--space-1);
    }

    .barrier-presets__glyph {
        display: inline-flex;
        align-items: center;
        width: 16px;
        height: 16px;
    }

    .barrier-presets__glyph :global(svg) {
        width: 16px;
        height: 16px;
    }
</style>
