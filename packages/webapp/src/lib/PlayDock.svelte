<script lang="ts">
    let {
        running,
        centerX,
        onToggle,
        onStep,
        onGen,
        onReset,
    }: {
        running: boolean;
        centerX: number;
        onToggle: () => void;
        onStep: () => void;
        onGen: () => void;
        onReset: () => void;
    } = $props();
</script>

<div class="playdock" style="left: {centerX}px">
    <!-- Primary play / pause toggle -->
    <button
        class="button button--pill button--filled playdock__primary"
        onclick={onToggle}
        aria-label={running ? "Pause simulation" : "Play simulation"}
    >
        {#if running}
            <!-- Pause bars -->
            <svg
                width="12"
                height="12"
                viewBox="0 0 12 12"
                fill="currentColor"
                aria-hidden="true"
            >
                <rect x="2" y="1" width="3" height="10" rx="0.5" />
                <rect x="7" y="1" width="3" height="10" rx="0.5" />
            </svg>
            Pause
        {:else}
            <!-- Play triangle -->
            <svg
                width="12"
                height="12"
                viewBox="0 0 12 12"
                fill="currentColor"
                aria-hidden="true"
            >
                <polygon points="2,1 11,6 2,11" />
            </svg>
            Play
        {/if}
    </button>

    <div class="playdock__divider" aria-hidden="true"></div>

    <!-- Step one tick -->
    <button
        class="button button--pill button--ghost"
        onclick={onStep}
        aria-label="Step one simulation tick"
    >
        Step
        <svg
            width="10"
            height="10"
            viewBox="0 0 10 10"
            fill="none"
            stroke="currentColor"
            stroke-width="1.5"
            aria-hidden="true"
        >
            <polyline points="2,2 7,5 2,8" />
        </svg>
    </button>

    <!-- Advance one generation -->
    <button
        class="button button--pill button--ghost"
        onclick={onGen}
        aria-label="Advance one generation"
    >
        Gen
        <svg
            width="10"
            height="10"
            viewBox="0 0 10 10"
            fill="none"
            stroke="currentColor"
            stroke-width="1.5"
            aria-hidden="true"
        >
            <polyline points="2,2 7,5 2,8" />
        </svg>
    </button>

    <div class="playdock__divider" aria-hidden="true"></div>

    <!-- Reset simulation -->
    <button
        class="button button--pill button--ghost button--icon"
        onclick={onReset}
        aria-label="Reset simulation"
    >
        <svg
            width="14"
            height="14"
            viewBox="0 0 14 14"
            fill="none"
            stroke="currentColor"
            stroke-width="1.5"
            stroke-linecap="round"
            stroke-linejoin="round"
            aria-hidden="true"
        >
            <path d="M2.5 7a4.5 4.5 0 1 1 1 2.8" />
            <polyline points="1,7.5 2.5,9.5 4,7.5" />
        </svg>
    </button>
</div>

<style>
    .playdock {
        position: fixed;
        top: 4.75rem;
        transform: translateX(-50%);
        z-index: 25;
        display: flex;
        align-items: center;
        gap: var(--space-1);
        padding: var(--space-1);
        border-radius: var(--radius-pill);
        background: var(--color-surface);
        border: 1px solid var(--color-border);
        box-shadow: var(--shadow-floating);
        transition: left 0.26s cubic-bezier(0.2, 0.7, 0.3, 1);
    }

    .playdock__primary {
        height: 2.25rem;
        padding: 0 var(--space-4);
    }

    .playdock__divider {
        width: 1px;
        height: 1.25rem;
        background: var(--color-border-subtle);
        margin: 0 var(--space-1);
        flex-shrink: 0;
    }
</style>
