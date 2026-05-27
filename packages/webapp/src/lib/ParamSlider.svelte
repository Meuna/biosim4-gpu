<script lang="ts">
    // ParamSlider — reusable slider knob with an editable value display.
    // Click the value display to switch to a number input; confirm with Enter
    // or blur, cancel with Escape.

    interface Props {
        label: string;
        hint?: string;
        min: number;
        max: number;
        step: number;
        value: number;
        format?: (val: number) => string;
        onchange: (val: number) => void;
    }
    const { label, hint, min, max, step, value, format, onchange }: Props =
        $props();

    let editing = $state(false);
    let editVal = $state("");
    let editInput = $state<HTMLInputElement | undefined>();

    $effect(() => {
        if (editing && editInput) {
            editInput.focus();
        }
    });

    function display(v: number): string {
        return format ? format(v) : String(v);
    }

    function startEdit(): void {
        editVal = String(value);
        editing = true;
    }

    function commitEdit(): void {
        const n = parseFloat(editVal);
        if (!isNaN(n)) {
            onchange(Math.max(min, Math.min(max, n)));
        }
        editing = false;
    }

    function cancelEdit(): void {
        editing = false;
    }
</script>

<div class="field-row">
    <div class="param-slider__header">
        <span class="field-label">{label}</span>
        {#if hint}<span class="field-hint">{hint}</span>{/if}
    </div>
    <div class="param-slider__row">
        <input
            type="range"
            class="param-slider__range"
            {min}
            {max}
            {step}
            {value}
            oninput={(e) => {
                onchange(parseFloat((e.target as HTMLInputElement).value));
            }}
            aria-label={label}
        />
        {#if editing}
            <input
                type="number"
                class="param-slider__edit"
                {min}
                {max}
                {step}
                bind:value={editVal}
                bind:this={editInput}
                onblur={commitEdit}
                onkeydown={(e) => {
                    if (e.key === "Enter") commitEdit();
                    if (e.key === "Escape") cancelEdit();
                }}
                aria-label={"Edit " + label}
            />
        {:else}
            <button
                type="button"
                class="param-slider__val"
                onclick={startEdit}
                aria-label={"Edit " + label}
                title="Click to edit"
            >
                {display(value)}
            </button>
        {/if}
    </div>
</div>

<style>
    .param-slider__header {
        display: flex;
        justify-content: space-between;
        align-items: baseline;
        margin-bottom: var(--space-2);
    }

    .param-slider__header .field-label {
        margin-bottom: 0;
    }

    .param-slider__row {
        display: flex;
        align-items: center;
        gap: var(--space-3);
    }

    .param-slider__range {
        flex: 1;
        accent-color: var(--color-accent);
    }

    .param-slider__val {
        font-family: var(--font-mono);
        font-size: var(--text-sm);
        color: var(--color-text);
        min-width: 3.5rem;
        text-align: right;
        background: none;
        border: none;
        cursor: pointer;
        padding: 0;
        border-bottom: 1px dashed transparent;
        transition: border-color 0.1s;
    }

    .param-slider__val:hover {
        border-bottom-color: var(--color-text-muted);
    }

    .param-slider__val:focus-visible {
        outline: 2px solid var(--color-accent);
        outline-offset: 2px;
    }

    .param-slider__edit {
        font-family: var(--font-mono);
        font-size: var(--text-sm);
        color: var(--color-text);
        background: var(--color-surface-alt);
        border: 1px solid var(--color-accent);
        border-radius: var(--radius-sm);
        width: 4.5rem;
        text-align: right;
        padding: 0 var(--space-1);
    }
</style>
