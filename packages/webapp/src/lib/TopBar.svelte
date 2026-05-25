<script lang="ts">
    let {
        running,
        gen,
        step,
        stepsPerGen,
    }: {
        running: boolean;
        gen: number;
        step: number;
        stepsPerGen: number;
    } = $props();
</script>

<header class="topbar">
    <div class="topbar__left">
        <span class="topbar__brand">biosim4-gpu</span>
        <span class="topbar__subtitle small-caps">visualizer · v0.1</span>
    </div>

    <div class="topbar__center">
        {#if running}
            <div class="status-chip status-chip--running">
                <span class="pulse-dot" aria-hidden="true"></span>
                <span class="status-chip__text">
                    running · gen {gen} · step {step}/{stepsPerGen}
                </span>
            </div>
        {:else}
            <div class="status-chip status-chip--idle">
                <span class="idle-dot" aria-hidden="true"></span>
                <span class="status-chip__text">idle</span>
            </div>
        {/if}
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

    .status-chip {
        display: inline-flex;
        align-items: center;
        gap: var(--space-2);
        padding: var(--space-1) var(--space-3);
        border-radius: var(--radius-pill);
        border: 1px solid var(--color-border-subtle);
        font-family: var(--font-mono);
        font-size: 0.75rem;
    }

    .status-chip--running {
        background: var(--color-accent-surface);
        border-color: var(--color-accent-border);
        color: var(--color-accent-text);
    }

    .status-chip--idle {
        color: var(--color-text-muted);
    }

    .status-chip__text {
        white-space: nowrap;
    }

    .pulse-dot {
        width: 7px;
        height: 7px;
        border-radius: 50%;
        background: var(--color-accent);
        flex-shrink: 0;
        animation: pulse 1.6s ease-out infinite;
    }

    .idle-dot {
        width: 7px;
        height: 7px;
        border-radius: 50%;
        border: 1.5px solid var(--color-text-muted);
        flex-shrink: 0;
    }

    @keyframes pulse {
        0% {
            opacity: 1;
            transform: scale(1);
        }
        50% {
            opacity: 0.5;
            transform: scale(0.85);
        }
        100% {
            opacity: 1;
            transform: scale(1);
        }
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
