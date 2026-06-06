<script lang="ts" generics="T">
    import { Infinity as InfinityIcon } from "lucide-svelte";

    const GLYPH_ICONS: Record<string, typeof InfinityIcon> = {
        infinity: InfinityIcon,
    };

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
            {@const glyphKey = stop.label.startsWith("glyph:")
                ? stop.label.slice(6)
                : null}
            {@const GlyphIcon =
                glyphKey !== null ? (GLYPH_ICONS[glyphKey] ?? null) : null}
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
                    {#if GlyphIcon}
                        <GlyphIcon size={12} aria-hidden="true" />
                    {:else}
                        {stop.label}
                    {/if}
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
        height: 1.5rem;
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
        border-radius: 9999px;
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
        width: 0.5rem;
        height: 0.5rem;
        border-radius: 9999px;
        border: 1.5px solid var(--color-border);
        background: var(--color-surface);
        transform: translateY(-50%);
        transition:
            background 0.1s,
            border-color 0.1s;
    }

    .discrete-slider__stop--active .discrete-slider__circle {
        background: var(--color-text);
        border-color: var(--color-text);
    }

    .discrete-slider__label {
        position: absolute;
        bottom: 0;
        display: flex;
        align-items: center;
        justify-content: center;
        font-family: var(--font-mono);
        font-size: 0.625rem;
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
