<script lang="ts" generics="T">
    let {
        stops,
        value,
        onChange,
        ariaLabel = "Select option",
    }: {
        stops: { value: T; label: string; ariaLabel?: string }[];
        value: T;
        onChange: (v: T) => void;
        ariaLabel?: string;
    } = $props();

    const activeIndex = $derived(
        Math.max(
            0,
            stops.findIndex((s) => s.value === value),
        ),
    );
    const knobPct = $derived((activeIndex / (stops.length - 1)) * 100);
</script>

<div class="discrete-slider" role="radiogroup" aria-label={ariaLabel}>
    <div class="discrete-slider__track">
        <div class="discrete-slider__rail"></div>
        {#each stops as stop, i}
            <button
                class="discrete-slider__stop"
                class:discrete-slider__stop--active={i === activeIndex}
                style="left: {(i / (stops.length - 1)) * 100}%"
                role="radio"
                aria-checked={i === activeIndex}
                aria-label={stop.ariaLabel ?? stop.label}
                onclick={() => onChange(stop.value)}
            >
                <div class="discrete-slider__circle"></div>
                <span class="discrete-slider__label">
                    {stop.label}
                </span>
            </button>
        {/each}
        <div
            class="discrete-slider__knob"
            style="left: {knobPct}%"
            aria-hidden="true"
        ></div>
    </div>
</div>

<style>
    /*
     * Layout: rail, circles, and knob all sit on the 50% axis so the rail
     * stays aligned with the vertical centre of the surrounding toolbar row.
     * Labels hang below the circle bottom via calc(50% + radius + gap) and
     * overflow the 2rem container visually — overflow: visible is the default.
     */
    .discrete-slider {
        display: inline-flex;
        align-items: center;
        width: 8rem;
        height: 2rem;
        padding: 0 var(--space-2);
    }

    .discrete-slider__track {
        position: relative;
        width: 100%;
        height: 2rem;
    }

    .discrete-slider__rail {
        position: absolute;
        left: 0;
        right: 0;
        top: 50%;
        height: 1px;
        background: var(--color-border-subtle);
        transform: translateY(-50%);
    }

    .discrete-slider__knob {
        position: absolute;
        top: 50%;
        width: 0.625rem;
        height: 0.625rem;
        border-radius: var(--radius-pill);
        background: var(--color-text);
        transform: translate(-50%, -50%);
        transition: left 0.2s ease;
        pointer-events: none;
        z-index: 2;
    }

    .discrete-slider__stop {
        position: absolute;
        top: 0;
        height: 100%;
        transform: translateX(-50%);
        display: flex;
        flex-direction: column;
        align-items: center;
        justify-content: center;
        background: none;
        border: none;
        cursor: pointer;
        padding: 0 var(--space-1);
        z-index: 1;
    }

    .discrete-slider__stop:focus-visible {
        outline: 2px solid var(--color-accent);
        outline-offset: 2px;
        border-radius: var(--radius-sm);
    }

    .discrete-slider__circle {
        position: absolute;
        top: 50%;
        width: 1rem;
        height: 1rem;
        border-radius: var(--radius-pill);
        border: 1.5px solid var(--color-border);
        background: var(--color-surface);
        transform: translateY(-50%);
    }

    .discrete-slider__stop--active .discrete-slider__circle {
        border-color: var(--color-text);
        border: 2px solid var(--color-border);
    }

    .discrete-slider__label {
        position: absolute;
        /* circle bottom = 50% + 0.5rem; add 0.25rem gap */
        top: calc(50% + 0.5rem);
        display: flex;
        align-items: center;
        justify-content: center;
        font-family: var(--font-mono);
        font-size: var(--text-2xs);
        color: var(--color-text-muted);
        white-space: nowrap;
        letter-spacing: 0.03em;
        pointer-events: none;
        transition: color 0.1s;
    }

    .discrete-slider__stop--active .discrete-slider__label {
        color: var(--color-text);
    }
</style>
