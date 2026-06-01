<script lang="ts">
    import type { Snippet } from "svelte";
    import { PanelRightClose } from "lucide-svelte";

    let {
        open,
        activeTab,
        hasSelection,
        onTabChange,
        onClose,
        sim,
        cell,
    }: {
        open: boolean;
        activeTab: "sim" | "cell";
        hasSelection: boolean;
        onTabChange: (t: "sim" | "cell") => void;
        onClose?: () => void;
        sim?: Snippet;
        cell?: Snippet;
    } = $props();
</script>

{#if open}
    <aside class="rail" aria-label="Configuration panel">
        <!-- Tab bar -->
        <div class="rail__tabs" role="tablist">
            <button
                class="rail__tab"
                class:rail__tab--active={activeTab === "sim"}
                role="tab"
                aria-selected={activeTab === "sim"}
                aria-controls="rail-panel-sim"
                onclick={() => onTabChange("sim")}
            >
                Simulation
            </button>
            <button
                class="rail__tab"
                class:rail__tab--active={activeTab === "cell"}
                role="tab"
                aria-selected={activeTab === "cell"}
                aria-controls="rail-panel-cell"
                onclick={() => onTabChange("cell")}
            >
                {#if hasSelection}
                    <span class="pulse-dot" aria-hidden="true"></span>
                {/if}
                Cell
            </button>
            {#if onClose}
                <button
                    class="rail__close"
                    onclick={onClose}
                    aria-label="Close panel"
                >
                    <PanelRightClose size={16} />
                </button>
            {/if}
        </div>

        <!-- Tab body -->
        <div class="rail__body">
            {#if activeTab === "sim"}
                <div
                    id="rail-panel-sim"
                    role="tabpanel"
                    aria-labelledby="tab-sim"
                >
                    {#if sim}{@render sim()}{/if}
                </div>
            {:else}
                <div
                    id="rail-panel-cell"
                    role="tabpanel"
                    aria-labelledby="tab-cell"
                >
                    {#if cell}{@render cell()}{/if}
                </div>
            {/if}
        </div>
    </aside>
{/if}

<style>
    .rail {
        position: fixed;
        right: 0;
        top: 3.5rem;
        bottom: 0;
        width: 23.75rem;
        z-index: 22;
        background: var(--color-surface-alt);
        border-left: 1px solid var(--color-border-subtle);
        display: flex;
        flex-direction: column;
        animation: slideIn 0.26s cubic-bezier(0.2, 0.7, 0.3, 1);
    }

    @keyframes slideIn {
        from {
            transform: translateX(100%);
        }
        to {
            transform: translateX(0);
        }
    }

    @media (max-width: 760px) {
        .rail {
            width: 100vw;
        }
    }

    /* ── Tab bar ── */
    .rail__tabs {
        display: flex;
        height: 3.5rem;
        border-bottom: 1px solid var(--color-border-subtle);
        flex-shrink: 0;
    }

    .rail__tab {
        flex: 1;
        display: flex;
        align-items: center;
        justify-content: center;
        gap: var(--space-2);
        border: 0;
        background: transparent;
        cursor: pointer;
        font-family: var(--font-mono);
        font-size: 0.75rem;
        text-transform: uppercase;
        letter-spacing: 0.1em;
        color: var(--color-text-muted);
        position: relative;
        transition: color 0.1s;
    }

    .rail__tab:hover {
        color: var(--color-text);
    }

    .rail__tab--active {
        color: var(--color-text);
        font-weight: 600;
    }

    /* Active underline indicator */
    .rail__tab--active::after {
        content: "";
        position: absolute;
        bottom: 0;
        left: var(--space-4);
        right: var(--space-4);
        height: 2px;
        background: var(--color-text);
    }

    /* Pulse dot on Cell tab when agent is selected */
    .pulse-dot {
        width: 7px;
        height: 7px;
        border-radius: 50%;
        background: var(--color-accent);
        flex-shrink: 0;
        animation: pulse 1.6s ease-out infinite;
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

    /* Close icon button — right side of tab bar, separated by a border */
    .rail__close {
        flex-shrink: 0;
        width: 3rem;
        height: 100%;
        display: flex;
        align-items: center;
        justify-content: center;
        border: 0;
        border-left: 1px solid var(--color-border-subtle);
        background: transparent;
        cursor: pointer;
        color: var(--color-text-muted);
        transition: color 0.1s;
    }

    .rail__close:hover {
        color: var(--color-text);
    }

    .rail__close:focus-visible {
        outline: 2px solid var(--color-accent);
        outline-offset: -2px;
    }

    /* ── Tab body ── */
    .rail__body {
        flex: 1;
        overflow-y: auto;
    }
</style>
