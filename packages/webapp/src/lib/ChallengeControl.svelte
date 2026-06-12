<script lang="ts">
    // ChallengeControl — dropdown + per-kind parameter controls for the
    // simulation challenge spec.
    import type { ChallengeSpec } from "../workers/sim.worker";
    import ParamSlider from "./ParamSlider.svelte";

    interface Props {
        value: ChallengeSpec;
        disabled?: boolean;
        onchange: (spec: ChallengeSpec) => void;
    }
    const { value, disabled = false, onchange }: Props = $props();

    // Human-readable labels for each challenge kind, in dropdown order.
    const KINDS: { kind: ChallengeSpec["kind"]; label: string }[] = [
        { kind: "x_band", label: "X-band" },
        { kind: "disc", label: "Disc" },
        { kind: "corners", label: "Corners" },
        { kind: "neighbor_count", label: "Neighbor count" },
        { kind: "center_sparse", label: "Center sparse" },
        { kind: "against_wall", label: "Against wall" },
        { kind: "migrate_distance", label: "Migrate distance" },
        { kind: "touch_any_wall", label: "Touch any wall" },
        { kind: "radioactive_walls", label: "Radioactive walls" },
        { kind: "pairs", label: "Pairs" },
        { kind: "location_sequence", label: "Location sequence" },
        { kind: "near_barrier", label: "Near barrier" },
        { kind: "altruism", label: "Altruism" },
    ];

    // Default parameter values per kind. Positional fields are fractions
    // of grid size in [0, 1]; they match the WASM defaults in bindings.c.
    function defaultSpec(kind: ChallengeSpec["kind"]): ChallengeSpec {
        switch (kind) {
            case "x_band":
                return { kind, xMin: 0.5, xMax: 1.0, mirror: false };
            case "disc":
                return { kind, x: 0.5, y: 0.5, radius: 0.25, weighted: false };
            case "corners":
                return { kind, radius: 0.25, weighted: false };
            case "neighbor_count":
                return {
                    kind,
                    radius: 0.25,
                    minN: 0,
                    maxN: 10,
                    excludeBorder: false,
                };
            case "center_sparse":
                return {
                    kind,
                    x: 0.5,
                    y: 0.5,
                    outerR: 0.25,
                    innerR: 0.0,
                    minN: 0,
                    maxN: 5,
                    weighted: false,
                };
            case "near_barrier":
                return { kind, radius: 0.25 };
            case "location_sequence":
                return { kind, radius: 0.25 };
            default:
                return { kind } as ChallengeSpec;
        }
    }

    function handleKindChange(e: Event): void {
        const kind = (e.target as HTMLSelectElement)
            .value as ChallengeSpec["kind"];
        onchange(defaultSpec(kind));
    }

    function emit(patch: Partial<ChallengeSpec>): void {
        onchange({ ...value, ...patch } as ChallengeSpec);
    }
</script>

<!-- Kind selector ─────────────────────────────────────────────────────────── -->
<div class="field-row challenge-control__kind-row">
    <label
        class="field-label challenge-control__kind-label"
        for="challenge-kind"
    >
        Kind
    </label>
    <select
        id="challenge-kind"
        class="challenge-control__select"
        value={value.kind}
        {disabled}
        onchange={handleKindChange}
        aria-label="Challenge kind"
    >
        {#each KINDS as { kind, label }}
            <option value={kind}>{label}</option>
        {/each}
    </select>
</div>

<!-- Per-kind parameter controls ────────────────────────────────────────────── -->

{#if value.kind === "x_band"}
    <ParamSlider
        label="X min"
        hint="fraction"
        min={0}
        max={1}
        step={0.01}
        value={value.xMin}
        format={(v) => v.toFixed(2)}
        {disabled}
        onchange={(v) => emit({ xMin: v })}
    />
    <ParamSlider
        label="X max"
        hint="fraction"
        min={0}
        max={1}
        step={0.01}
        value={value.xMax}
        format={(v) => v.toFixed(2)}
        {disabled}
        onchange={(v) => emit({ xMax: v })}
    />
    <div class="field-row toggle-row">
        <label class="toggle-label">
            <input
                type="checkbox"
                class="toggle-checkbox"
                checked={value.mirror}
                {disabled}
                onchange={(e) =>
                    emit({ mirror: (e.target as HTMLInputElement).checked })}
                aria-label="Mirror"
            />
            <span class="field-label">Mirror</span>
        </label>
    </div>
{/if}

{#if value.kind === "disc"}
    <ParamSlider
        label="X"
        hint="fraction"
        min={0}
        max={1}
        step={0.01}
        value={value.x}
        format={(v) => v.toFixed(2)}
        {disabled}
        onchange={(v) => emit({ x: v })}
    />
    <ParamSlider
        label="Y"
        hint="fraction"
        min={0}
        max={1}
        step={0.01}
        value={value.y}
        format={(v) => v.toFixed(2)}
        {disabled}
        onchange={(v) => emit({ y: v })}
    />
    <ParamSlider
        label="Radius"
        hint="fraction"
        min={0.01}
        max={0.75}
        step={0.01}
        value={value.radius}
        format={(v) => v.toFixed(2)}
        {disabled}
        onchange={(v) => emit({ radius: v })}
    />
    <div class="field-row toggle-row">
        <label class="toggle-label">
            <input
                type="checkbox"
                class="toggle-checkbox"
                checked={value.weighted}
                {disabled}
                onchange={(e) =>
                    emit({ weighted: (e.target as HTMLInputElement).checked })}
                aria-label="Weighted"
            />
            <span class="field-label">Weighted</span>
        </label>
    </div>
{/if}

{#if value.kind === "corners"}
    <ParamSlider
        label="Radius"
        hint="fraction"
        min={0.01}
        max={0.5}
        step={0.01}
        value={value.radius}
        format={(v) => v.toFixed(2)}
        {disabled}
        onchange={(v) => emit({ radius: v })}
    />
    <div class="field-row toggle-row">
        <label class="toggle-label">
            <input
                type="checkbox"
                class="toggle-checkbox"
                checked={value.weighted}
                {disabled}
                onchange={(e) =>
                    emit({ weighted: (e.target as HTMLInputElement).checked })}
                aria-label="Weighted"
            />
            <span class="field-label">Weighted</span>
        </label>
    </div>
{/if}

{#if value.kind === "neighbor_count"}
    <ParamSlider
        label="Radius"
        hint="fraction"
        min={0.01}
        max={0.5}
        step={0.01}
        value={value.radius}
        format={(v) => v.toFixed(2)}
        {disabled}
        onchange={(v) => emit({ radius: v })}
    />
    <ParamSlider
        label="Min neighbours"
        hint="agents"
        min={0}
        max={20}
        step={1}
        value={value.minN}
        {disabled}
        onchange={(v) => emit({ minN: v })}
    />
    <ParamSlider
        label="Max neighbours"
        hint="agents"
        min={0}
        max={20}
        step={1}
        value={value.maxN}
        {disabled}
        onchange={(v) => emit({ maxN: v })}
    />
    <div class="field-row toggle-row">
        <label class="toggle-label">
            <input
                type="checkbox"
                class="toggle-checkbox"
                checked={value.excludeBorder}
                {disabled}
                onchange={(e) =>
                    emit({
                        excludeBorder: (e.target as HTMLInputElement).checked,
                    })}
                aria-label="Exclude border"
            />
            <span class="field-label">Exclude border</span>
        </label>
    </div>
{/if}

{#if value.kind === "center_sparse"}
    <ParamSlider
        label="X"
        hint="fraction"
        min={0}
        max={1}
        step={0.01}
        value={value.x}
        format={(v) => v.toFixed(2)}
        {disabled}
        onchange={(v) => emit({ x: v })}
    />
    <ParamSlider
        label="Y"
        hint="fraction"
        min={0}
        max={1}
        step={0.01}
        value={value.y}
        format={(v) => v.toFixed(2)}
        {disabled}
        onchange={(v) => emit({ y: v })}
    />
    <ParamSlider
        label="Outer radius"
        hint="fraction"
        min={0.01}
        max={0.75}
        step={0.01}
        value={value.outerR}
        format={(v) => v.toFixed(2)}
        {disabled}
        onchange={(v) => emit({ outerR: v })}
    />
    <ParamSlider
        label="Inner radius"
        hint="fraction"
        min={0}
        max={0.75}
        step={0.01}
        value={value.innerR}
        format={(v) => v.toFixed(2)}
        {disabled}
        onchange={(v) => emit({ innerR: v })}
    />
    <ParamSlider
        label="Min neighbours"
        hint="agents"
        min={0}
        max={20}
        step={1}
        value={value.minN}
        {disabled}
        onchange={(v) => emit({ minN: v })}
    />
    <ParamSlider
        label="Max neighbours"
        hint="agents"
        min={0}
        max={20}
        step={1}
        value={value.maxN}
        {disabled}
        onchange={(v) => emit({ maxN: v })}
    />
    <div class="field-row toggle-row">
        <label class="toggle-label">
            <input
                type="checkbox"
                class="toggle-checkbox"
                checked={value.weighted}
                {disabled}
                onchange={(e) =>
                    emit({ weighted: (e.target as HTMLInputElement).checked })}
                aria-label="Weighted"
            />
            <span class="field-label">Weighted</span>
        </label>
    </div>
{/if}

{#if value.kind === "near_barrier" || value.kind === "location_sequence"}
    <ParamSlider
        label="Radius"
        hint="fraction"
        min={0.01}
        max={0.5}
        step={0.01}
        value={value.radius}
        format={(v) => v.toFixed(2)}
        {disabled}
        onchange={(v) => emit({ radius: v })}
    />
{/if}

<style>
    .challenge-control__kind-row {
        align-items: center;
        gap: var(--space-3);
    }

    .challenge-control__kind-label {
        flex-shrink: 0;
    }

    .challenge-control__select {
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
</style>
