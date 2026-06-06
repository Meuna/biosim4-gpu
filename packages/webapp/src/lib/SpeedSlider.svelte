<script lang="ts">
    const STOPS: { fps: 0 | 1 | 25 | 50; label: string }[] = [
        { fps: 1, label: "1" },
        { fps: 25, label: "25" },
        { fps: 50, label: "50" },
        { fps: 0, label: "∞" },
    ];

    let {
        value,
        onChange,
    }: {
        value: 0 | 1 | 25 | 50;
        onChange: (fps: 0 | 1 | 25 | 50) => void;
    } = $props();

    const activeIndex = $derived(
        Math.max(
            0,
            STOPS.findIndex((s) => s.fps === value),
        ),
    );
    const knobPct = $derived((activeIndex / (STOPS.length - 1)) * 100);
</script>

<div class="speed-slider" role="group" aria-label="Simulation speed">
    <div class="speed-slider__track">
        <div class="speed-slider__rail"></div>
        <div class="speed-slider__knob" style="left: {knobPct}%"></div>
        {#each STOPS as stop, i}
            <button
                class="speed-slider__stop"
                class:speed-slider__stop--active={i === activeIndex}
                style="left: {(i / (STOPS.length - 1)) * 100}%"
                onclick={() => onChange(stop.fps)}
                aria-label={stop.fps === 0
                    ? "Set speed to unlimited"
                    : `Set speed to ${stop.fps} fps`}
                aria-pressed={i === activeIndex}
            >
                <span class="speed-slider__label">{stop.label}</span>
            </button>
        {/each}
    </div>
</div>

<style>
    .speed-slider {
        display: inline-flex;
        align-items: center;
        width: 8rem;
        height: 2rem;
        padding: 0 var(--space-2);
    }

    .speed-slider__track {
        position: relative;
        width: 100%;
        height: 1.5rem;
    }

    .speed-slider__rail {
        position: absolute;
        left: 0;
        right: 0;
        top: 50%;
        height: 1px;
        background: var(--color-border-subtle);
        transform: translateY(-50%);
    }

    .speed-slider__knob {
        position: absolute;
        top: 50%;
        width: 0.5rem;
        height: 0.5rem;
        border-radius: 9999px;
        background: var(--color-text);
        transform: translate(-50%, -50%);
        transition: left 0.2s ease;
        pointer-events: none;
    }

    .speed-slider__stop {
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
    }

    .speed-slider__stop:focus-visible {
        outline: 2px solid var(--color-accent);
        outline-offset: 2px;
        border-radius: var(--radius-sm);
    }

    .speed-slider__label {
        position: absolute;
        bottom: 0;
        font-family: var(--font-mono);
        font-size: 0.5rem;
        color: var(--color-text-muted);
        white-space: nowrap;
        letter-spacing: 0.03em;
        pointer-events: none;
        transition: color 0.1s;
    }

    .speed-slider__stop--active .speed-slider__label {
        color: var(--color-text);
    }
</style>
