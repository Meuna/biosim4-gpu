<script lang="ts">
    // BarrierControl — add/remove/configure barriers for the simulation.
    import type {
        BarrierSpec,
        BarrierKind,
        CornerQuadrant,
    } from "../workers/sim.worker";
    import ParamSlider from "./ParamSlider.svelte";
    import { Dices, Trash2 } from "lucide-svelte";

    interface Props {
        value: BarrierSpec[];
        disabled?: boolean;
        onchange: (barriers: BarrierSpec[]) => void;
    }
    const { value, disabled = false, onchange }: Props = $props();

    const KINDS: { kind: BarrierKind; label: string }[] = [
        { kind: "hbar", label: "Horizontal bar" },
        { kind: "vbar", label: "Vertical bar" },
        { kind: "square", label: "Square" },
        { kind: "circle", label: "Circle" },
        { kind: "corner", label: "Corner" },
    ];

    // Labels are screen-oriented: the canvas draws grid-y downward, so the core
    // quadrant (y up) maps to the opposite vertical half on screen.
    const QUADRANTS: { quadrant: CornerQuadrant; label: string }[] = [
        { quadrant: "se", label: "Top-right" },
        { quadrant: "sw", label: "Top-left" },
        { quadrant: "ne", label: "Bottom-right" },
        { quadrant: "nw", label: "Bottom-left" },
    ];

    const DEFAULT_BARRIER: BarrierSpec = {
        kind: "hbar",
        x: 0.5,
        y: 0.5,
        length: 0.25,
        width: 0.02,
        quadrant: "ne",
    };

    function hasWidth(kind: BarrierKind): boolean {
        return kind === "hbar" || kind === "vbar" || kind === "corner";
    }

    function addBarrier(): void {
        onchange([...value, { ...DEFAULT_BARRIER }]);
    }

    function removeBarrier(i: number): void {
        collapsed.splice(i, 1);
        onchange(value.filter((_, idx) => idx !== i));
    }

    function patchBarrier(i: number, patch: Partial<BarrierSpec>): void {
        const updated = value.map((b, idx) =>
            idx === i ? ({ ...b, ...patch } as BarrierSpec) : b,
        );
        onchange(updated);
    }

    function shuffleBarrier(i: number): void {
        patchBarrier(i, {
            x: Math.random(),
            y: Math.random(),
            length: 0.05 + Math.random() * 0.45,
            width: hasWidth(value[i].kind)
                ? 0.005 + Math.random() * 0.095
                : null,
        });
    }

    // Per-barrier collapse state; grows when barriers are added.
    let collapsed = $state<boolean[]>([]);
    $effect(() => {
        if (collapsed.length !== value.length) {
            collapsed = value.map((_, i) => collapsed[i] ?? false);
        }
    });

    function toggleCollapse(i: number): void {
        collapsed[i] = !collapsed[i];
    }

    function barrierSummary(b: BarrierSpec): string {
        const fmt = (v: number | null): string =>
            v !== null ? v.toFixed(2) : "?";
        const parts = [fmt(b.x), fmt(b.y), fmt(b.length)];
        if (b.width !== null) {
            parts.push(b.width.toFixed(3));
        }
        return `${b.kind}(${parts.join(", ")})`;
    }
</script>

{#each value as barrier, i}
    <div class="barrier-control__item">
        <button
            class="barrier-control__collapse-trigger"
            {disabled}
            onclick={() => toggleCollapse(i)}
            aria-label={`Toggle barrier ${i + 1}`}
            aria-expanded={!collapsed[i]}
        ></button>

        {#if collapsed[i]}
            <div class="field-row barrier-control__collapsed-summary">
                <span class="field-label barrier-control__index">#{i + 1}</span>
                <code class="barrier-control__summary-code"
                    >{barrierSummary(barrier)}</code
                >
            </div>
        {:else}
            <!-- Kind + remove row -->
            <div class="field-row barrier-control__header-row">
                <span class="field-label barrier-control__index">#{i + 1}</span>
                <select
                    class="barrier-control__select"
                    value={barrier.kind}
                    {disabled}
                    onchange={(e) =>
                        patchBarrier(i, {
                            kind: (e.target as HTMLSelectElement)
                                .value as BarrierKind,
                            width: hasWidth(
                                (e.target as HTMLSelectElement)
                                    .value as BarrierKind,
                            )
                                ? (barrier.width ?? DEFAULT_BARRIER.width)
                                : null,
                        })}
                    aria-label={`Barrier ${i + 1} kind`}
                >
                    {#each KINDS as { kind, label }}
                        <option value={kind}>{label}</option>
                    {/each}
                </select>
                <button
                    class="button button--ghost barrier-control__small-button"
                    {disabled}
                    onclick={() => shuffleBarrier(i)}
                    aria-label={`Shuffle barrier ${i + 1}`}
                >
                    <Dices size={13} />
                </button>
                <button
                    class="button button--ghost barrier-control__small-button"
                    {disabled}
                    onclick={() => removeBarrier(i)}
                    aria-label={`Remove barrier ${i + 1}`}
                >
                    <Trash2 size={13} />
                </button>
            </div>

            <!-- Position X -->
            <ParamSlider
                label="Position X"
                hint="fraction"
                min={0}
                max={1}
                step={0.01}
                value={barrier.x ?? 0.5}
                format={(v) => v.toFixed(2)}
                {disabled}
                onchange={(v) => patchBarrier(i, { x: v })}
            />

            <!-- Position Y -->
            <ParamSlider
                label="Position Y"
                hint="fraction"
                min={0}
                max={1}
                step={0.01}
                value={barrier.y ?? 0.5}
                format={(v) => v.toFixed(2)}
                {disabled}
                onchange={(v) => patchBarrier(i, { y: v })}
            />

            <!-- Length -->
            <ParamSlider
                label="Length"
                hint="fraction"
                min={0.01}
                max={1}
                step={0.01}
                value={barrier.length ?? 0.25}
                format={(v) => v.toFixed(2)}
                {disabled}
                onchange={(v) => patchBarrier(i, { length: v })}
            />

            <!-- Quadrant — corner only -->
            {#if barrier.kind === "corner"}
                <div class="field-row barrier-control__quadrant-row">
                    <span class="field-label">Quadrant</span>
                    <select
                        class="barrier-control__select"
                        value={barrier.quadrant}
                        {disabled}
                        onchange={(e) =>
                            patchBarrier(i, {
                                quadrant: (e.target as HTMLSelectElement)
                                    .value as CornerQuadrant,
                            })}
                        aria-label={`Barrier ${i + 1} quadrant`}
                    >
                        {#each QUADRANTS as { quadrant, label }}
                            <option value={quadrant}>{label}</option>
                        {/each}
                    </select>
                </div>
            {/if}

            <!-- Width — bars and corners -->
            {#if hasWidth(barrier.kind)}
                <ParamSlider
                    label="Width"
                    hint="fraction"
                    min={0.005}
                    max={0.25}
                    step={0.005}
                    value={barrier.width ?? DEFAULT_BARRIER.width!}
                    format={(v) => v.toFixed(3)}
                    {disabled}
                    onchange={(v) => patchBarrier(i, { width: v })}
                />
            {/if}
        {/if}
    </div>
{/each}

<div class="field-row">
    <button
        class="button button--ghost barrier-control__add"
        {disabled}
        onclick={addBarrier}
        aria-label="Add barrier"
    >
        + Add barrier
    </button>
</div>

<style>
    .barrier-control__item {
        position: relative;
        border-left: 2px solid var(--color-border);
        padding-left: var(--space-3);
        margin-bottom: var(--space-3);
    }

    .barrier-control__collapse-trigger {
        position: absolute;
        left: 0;
        top: 0;
        bottom: 0;
        width: var(--space-2);
        background: none;
        border: none;
        cursor: pointer;
        padding: 0;
    }

    .barrier-control__collapse-trigger:hover {
        background: color-mix(in srgb, var(--color-border) 30%, transparent);
    }

    .barrier-control__collapsed-summary {
        align-items: center;
        gap: var(--space-2);
        padding: var(--space-1) 0;
    }

    .barrier-control__summary-code {
        font-family: var(--font-mono);
        font-size: var(--text-sm);
        color: var(--color-text-muted);
    }

    .barrier-control__header-row {
        align-items: center;
        gap: var(--space-2);
        margin-bottom: var(--space-1);
    }

    .barrier-control__quadrant-row {
        align-items: center;
        gap: var(--space-2);
        margin-bottom: var(--space-1);
    }

    .barrier-control__index {
        flex-shrink: 0;
        color: var(--color-text-muted);
    }

    .barrier-control__select {
        flex: 1;
        background: var(--color-control-surface);
        border: 1px solid var(--color-border);
        border-radius: var(--radius-md);
        font-family: var(--font-sans);
        font-size: var(--text-sm);
        color: var(--color-text);
        padding: var(--space-1) var(--space-2);
        cursor: pointer;
        appearance: auto;
    }

    .barrier-control__small-button {
        flex-shrink: 0;
        padding: var(--space-1) var(--space-2);
        font-size: var(--text-sm);
    }

    .barrier-control__add {
        width: 100%;
        justify-content: center;
    }
</style>
