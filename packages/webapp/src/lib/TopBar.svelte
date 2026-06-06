<script lang="ts">
    import PlayDock from "./PlayDock.svelte";

    let {
        running,
        genComplete,
        genomIncompatible = false,
        targetSpeed,
        onToggle,
        onStep,
        onNextGen,
        onRewind,
        onClearGenom,
        onSetSpeed,
    }: {
        running: boolean;
        genComplete: boolean;
        genomIncompatible?: boolean;
        targetSpeed: number;
        onToggle: () => void;
        onStep: () => void;
        onNextGen: (autoPlay: boolean) => void;
        onRewind: (autoPlay: boolean) => void;
        onClearGenom: () => void;
        onSetSpeed: (fps: number) => void;
    } = $props();
</script>

<header class="topbar">
    <div class="topbar__left">
        <span class="topbar__brand">biosim4-gpu</span>
        <span class="topbar__subtitle small-caps">visualizer · v0.1</span>
    </div>

    <div class="topbar__center">
        <PlayDock
            {running}
            {genComplete}
            {genomIncompatible}
            {targetSpeed}
            {onToggle}
            {onStep}
            {onNextGen}
            {onRewind}
            {onClearGenom}
            {onSetSpeed}
        />
    </div>

    <div class="topbar__right">
        <a
            href="https://github.com/biosim4-gpu/biosim4-gpu"
            target="_blank"
            rel="noopener noreferrer"
            class="topbar__github"
        >
            <svg
                width="16"
                height="16"
                viewBox="0 0 16 16"
                fill="currentColor"
                aria-hidden="true"
            >
                <path
                    d="M8 0C3.58 0 0 3.58 0 8c0 3.54 2.29 6.53 5.47 7.59.4.07.55-.17.55-.38 0-.19-.01-.82-.01-1.49-2.01.37-2.53-.49-2.69-.94-.09-.23-.48-.94-.82-1.13-.28-.15-.68-.52-.01-.53.63-.01 1.08.58 1.23.82.72 1.21 1.87.87 2.33.66.07-.52.28-.87.51-1.07-1.78-.2-3.64-.89-3.64-3.95 0-.87.31-1.59.82-2.15-.08-.2-.36-1.02.08-2.12 0 0 .67-.21 2.2.82.64-.18 1.32-.27 2-.27.68 0 1.36.09 2 .27 1.53-1.04 2.2-.82 2.2-.82.44 1.1.16 1.92.08 2.12.51.56.82 1.27.82 2.15 0 3.07-1.87 3.75-3.65 3.95.29.25.54.73.54 1.48 0 1.07-.01 1.93-.01 2.2 0 .21.15.46.55.38A8.013 8.013 0 0016 8c0-4.42-3.58-8-8-8z"
                />
            </svg>
            <span>GitHub</span>
        </a>
    </div>
</header>

<style>
    .topbar {
        position: fixed;
        left: 0;
        right: 0;
        top: 0;
        height: 3.5rem;
        z-index: 20;
        display: flex;
        align-items: center;
        justify-content: space-between;
        padding: 0 var(--space-6);
        background: var(--color-surface-glass);
        backdrop-filter: blur(6px);
        border-bottom: 1px solid var(--color-border-subtle);
    }

    .topbar__left {
        display: flex;
        align-items: baseline;
        gap: var(--space-3);
    }

    .topbar__brand {
        font-family: var(--font-sans);
        font-size: 1.375rem;
        font-weight: 700;
        color: var(--color-text);
        letter-spacing: -0.02em;
    }

    /* .topbar__subtitle uses .small-caps from primitives.css */

    .topbar__center {
        position: absolute;
        left: 50%;
        transform: translateX(-50%);
    }

    .topbar__right {
        display: flex;
        align-items: center;
    }

    .topbar__github {
        display: inline-flex;
        align-items: center;
        gap: var(--space-2);
        font-family: var(--font-mono);
        font-size: 0.75rem;
        color: var(--color-text-muted);
        text-decoration: none;
        letter-spacing: 0.06em;
        transition: color 0.1s;
    }

    .topbar__github:hover {
        color: var(--color-text);
    }
</style>
