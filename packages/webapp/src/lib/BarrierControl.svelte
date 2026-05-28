<script lang="ts">
    // BarrierControl — add/remove/configure barriers for the simulation.
    import type {
        BarrierSpec,
        BarrierKind,
        ChallengeSpec,
    } from "../workers/sim.worker";
    import ParamSlider from "./ParamSlider.svelte";

    interface Props {
        value: BarrierSpec[];
        challengeKind?: ChallengeSpec["kind"];
        onchange: (barriers: BarrierSpec[]) => void;
    }
    const { value, challengeKind, onchange }: Props = $props();

    const KINDS: { kind: BarrierKind; label: string }[] = [
        { kind: "hbar", label: "Horizontal bar" },
        { kind: "vbar", label: "Vertical bar" },
        { kind: "square", label: "Square" },
        { kind: "circle", label: "Circle" },
    ];

    const DEFAULT_BARRIER: BarrierSpec = {
        kind: "hbar",
        x: 0.5,
        y: 0.5,
        length: 0.25,
        width: 0.02,
    };

    function hasWidth(kind: BarrierKind): boolean {
        return kind === "hbar" || kind === "vbar";
    }

    function addBarrier(): void {
        onchange([...value, { ...DEFAULT_BARRIER }]);
    }

    function removeBarrier(i: number): void {
        onchange(value.filter((_, idx) => idx !== i));
    }

    function patchBarrier(i: number, patch: Partial<BarrierSpec>): void {
        const updated = value.map((b, idx) =>
            idx === i ? ({ ...b, ...patch } as BarrierSpec) : b,
        );
        onchange(updated);
    }

    const needsBarriers = $derived(
        challengeKind === "near_barrier" ||
            challengeKind === "location_sequence",
    );
</script>

{#if needsBarriers && value.length === 0}
    <p class="barrier-control__warning">
        Near-barrier challenges require at least one barrier.
    </p>
{/if}

{#each value as barrier, i}
    <div class="barrier-control__item">
        <!-- Kind + remove row -->
        <div class="field-row barrier-control__header-row">
            <span class="field-label barrier-control__index">#{i + 1}</span>
            <select
                class="barrier-control__select"
                value={barrier.kind}
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
                class="button button--ghost barrier-control__remove"
                onclick={() => removeBarrier(i)}
                aria-label={`Remove barrier ${i + 1}`}
            >
                ✕
            </button>
        </div>

        <!-- Position X -->
        <div class="barrier-control__param-group">
            {#if barrier.x !== null}
                <ParamSlider
                    label="Position X"
                    hint="fraction"
                    min={0}
                    max={1}
                    step={0.01}
                    value={barrier.x}
                    format={(v) => v.toFixed(2)}
                    onchange={(v) => patchBarrier(i, { x: v })}
                />
            {:else}
                <div class="field-row barrier-control__random-placeholder">
                    <span class="field-label">Position X</span>
                    <span class="barrier-control__random-hint">random</span>
                </div>
            {/if}
            <label class="barrier-control__random-label toggle-label">
                <input
                    type="checkbox"
                    class="toggle-checkbox"
                    checked={barrier.x === null}
                    onchange={(e) =>
                        patchBarrier(i, {
                            x: (e.target as HTMLInputElement).checked
                                ? null
                                : 0.5,
                        })}
                    aria-label={`Barrier ${i + 1} X random`}
                />
                <span class="field-label">Random</span>
            </label>
        </div>

        <!-- Position Y -->
        <div class="barrier-control__param-group">
            {#if barrier.y !== null}
                <ParamSlider
                    label="Position Y"
                    hint="fraction"
                    min={0}
                    max={1}
                    step={0.01}
                    value={barrier.y}
                    format={(v) => v.toFixed(2)}
                    onchange={(v) => patchBarrier(i, { y: v })}
                />
            {:else}
                <div class="field-row barrier-control__random-placeholder">
                    <span class="field-label">Position Y</span>
                    <span class="barrier-control__random-hint">random</span>
                </div>
            {/if}
            <label class="barrier-control__random-label toggle-label">
                <input
                    type="checkbox"
                    class="toggle-checkbox"
                    checked={barrier.y === null}
                    onchange={(e) =>
                        patchBarrier(i, {
                            y: (e.target as HTMLInputElement).checked
                                ? null
                                : 0.5,
                        })}
                    aria-label={`Barrier ${i + 1} Y random`}
                />
                <span class="field-label">Random</span>
            </label>
        </div>

        <!-- Length -->
        <div class="barrier-control__param-group">
            {#if barrier.length !== null}
                <ParamSlider
                    label="Length"
                    hint="fraction"
                    min={0.01}
                    max={1}
                    step={0.01}
                    value={barrier.length}
                    format={(v) => v.toFixed(2)}
                    onchange={(v) => patchBarrier(i, { length: v })}
                />
            {:else}
                <div class="field-row barrier-control__random-placeholder">
                    <span class="field-label">Length</span>
                    <span class="barrier-control__random-hint">random</span>
                </div>
            {/if}
            <label class="barrier-control__random-label toggle-label">
                <input
                    type="checkbox"
                    class="toggle-checkbox"
                    checked={barrier.length === null}
                    onchange={(e) =>
                        patchBarrier(i, {
                            length: (e.target as HTMLInputElement).checked
                                ? null
                                : 0.25,
                        })}
                    aria-label={`Barrier ${i + 1} length random`}
                />
                <span class="field-label">Random</span>
            </label>
        </div>

        <!-- Width — bars only -->
        {#if hasWidth(barrier.kind)}
            <div class="barrier-control__param-group">
                {#if barrier.width !== null}
                    <ParamSlider
                        label="Width"
                        hint="fraction"
                        min={0.005}
                        max={0.25}
                        step={0.005}
                        value={barrier.width}
                        format={(v) => v.toFixed(3)}
                        onchange={(v) => patchBarrier(i, { width: v })}
                    />
                {:else}
                    <div class="field-row barrier-control__random-placeholder">
                        <span class="field-label">Width</span>
                        <span class="barrier-control__random-hint">random</span>
                    </div>
                {/if}
                <label class="barrier-control__random-label toggle-label">
                    <input
                        type="checkbox"
                        class="toggle-checkbox"
                        checked={barrier.width === null}
                        onchange={(e) =>
                            patchBarrier(i, {
                                width: (e.target as HTMLInputElement).checked
                                    ? null
                                    : 0.02,
                            })}
                        aria-label={`Barrier ${i + 1} width random`}
                    />
                    <span class="field-label">Random</span>
                </label>
            </div>
        {/if}
    </div>
{/each}

<div class="field-row">
    <button
        class="button button--ghost barrier-control__add"
        onclick={addBarrier}
        aria-label="Add barrier"
    >
        + Add barrier
    </button>
</div>

<style>
    .barrier-control__warning {
        font-family: var(--font-sans);
        font-size: var(--text-sm);
        color: var(--color-warn);
        margin: var(--space-1) 0;
    }

    .barrier-control__item {
        border-left: 2px solid var(--color-border);
        padding-left: var(--space-3);
        margin-bottom: var(--space-3);
    }

    .barrier-control__header-row {
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

    .barrier-control__remove {
        flex-shrink: 0;
        padding: var(--space-1) var(--space-2);
        font-size: var(--text-sm);
    }

    .barrier-control__param-group {
        display: flex;
        flex-direction: column;
        gap: var(--space-1);
    }

    .barrier-control__random-placeholder {
        justify-content: space-between;
        align-items: center;
    }

    .barrier-control__random-hint {
        font-family: var(--font-mono);
        font-size: var(--text-sm);
        color: var(--color-text-muted);
        font-style: italic;
    }

    .barrier-control__random-label {
        align-self: flex-start;
    }

    .barrier-control__add {
        width: 100%;
        justify-content: center;
    }
</style>
