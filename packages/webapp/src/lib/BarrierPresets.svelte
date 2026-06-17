<script lang="ts">
    // BarrierPresets — one-click pills that replace the barrier list with a
    // common layout. Kept separate from BarrierControl so the latter stays a
    // pure value/onchange control.
    import type { BarrierSpec, CornerQuadrant } from "../workers/sim.worker";
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
        return { kind, x, y, length, width: BAR_WIDTH, quadrant: "ne" };
    }

    function circle(x: number, y: number, radius: number): BarrierSpec {
        return {
            kind: "circle",
            x,
            y,
            length: radius,
            width: null,
            quadrant: "ne",
        };
    }

    function corner(
        quadrant: CornerQuadrant,
        x: number,
        y: number,
        length: number,
    ): BarrierSpec {
        return {
            kind: "corner",
            x: x,
            y: y,
            length: length,
            width: BAR_WIDTH,
            quadrant,
        };
    }

    function buildCross(): BarrierSpec[] {
        return [bar("hbar", 0.5, 0.5, 0.75), bar("vbar", 0.5, 0.5, 0.75)];
    }

    function buildVerticalSplit(): BarrierSpec[] {
        return [
            bar("vbar", 0.5, 0.05, 0.1),
            bar("vbar", 0.5, 0.5, 0.3),
            bar("vbar", 0.5, 0.95, 0.1),
            bar("vbar", 0.25, 0.25, 0.35),
            bar("vbar", 0.25, 0.75, 0.35),
            bar("vbar", 0.75, 0.25, 0.35),
            bar("vbar", 0.75, 0.75, 0.35),
        ];
    }

    // Inner brackets: vertices near the centre with arms reaching outward
    // toward the corners, so the open channels between them meet in a plus.
    function buildBarCross(): BarrierSpec[] {
        const arm = 0.3;
        return [
            corner("ne", 0.6, 0.4, arm),
            corner("nw", 0.4, 0.4, arm),
            corner("sw", 0.4, 0.6, arm),
            corner("se", 0.6, 0.6, arm),
        ];
    }

    // Brackets hug the outer corners with short arms, forming a box whose four
    // sides each have a gap in the middle.
    function buildSquare(): BarrierSpec[] {
        const arm = 0.25;
        return [
            corner("sw", 0.85, 0.15, arm),
            corner("se", 0.15, 0.15, arm),
            corner("ne", 0.15, 0.85, arm),
            corner("nw", 0.85, 0.85, arm),
        ];
    }

    function buildFiveDots(): BarrierSpec[] {
        const r = 0.08;
        return [
            circle(0.5, 0.5, r),
            circle(0.22, 0.22, r),
            circle(0.77, 0.22, r),
            circle(0.22, 0.77, r),
            circle(0.77, 0.77, r),
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
                    0.02,
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
                    <svg
                        viewBox="0 0 24 24"
                        fill="none"
                        stroke="currentColor"
                        stroke-width="2"
                        stroke-linecap="round"
                        stroke-linejoin="round"
                        version="1.1"
                    >
                        <path d="M5 12h14"></path>
                        <path d="M12 5v14"></path>
                    </svg>
                {:else if preset.id === "vsplit"}
                    <svg
                        viewBox="0 0 24 24"
                        fill="none"
                        stroke="currentColor"
                        stroke-width="2"
                        stroke-linecap="round"
                        stroke-linejoin="round"
                    >
                        <path d="M5 4v5" />
                        <path d="M5 15v5" />
                        <path d="M12 7v10" />
                        <path d="M19 4v5" />
                        <path d="M19 15v5" />
                    </svg>
                {:else if preset.id === "barcross"}
                    <svg
                        viewBox="0 0 24 24"
                        fill="none"
                        stroke="currentColor"
                        stroke-width="2"
                        stroke-linecap="round"
                        stroke-linejoin="round"
                    >
                        <path d="M3 9h5a1 1 0 0 0 1-1v-5" />
                        <path d="M3 15h5a1 1 0 0 1 1 1v5" />
                        <path d="M15 3v5a1 1 0 0 0 1 1h5" />
                        <path d="M15 21v-5a1 1 0 0 1 1-1h5" />
                    </svg>
                {:else if preset.id === "square"}
                    <svg
                        viewBox="0 0 24 24"
                        fill="none"
                        stroke="currentColor"
                        stroke-width="2"
                        stroke-linecap="round"
                        stroke-linejoin="round"
                    >
                        <path d="M4 9v-4a1 1 0 0 1 1-1h4" />
                        <path d="M4 15v4a1 1 0 0 0 1 1h4" />
                        <path d="M15 20h4a1 1 0 0 0 1-1v-4" />
                        <path d="M15 4h4a1 1 0 0 1 1 1v4" />
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
