<script lang="ts">
    import type { Snippet } from "svelte";

    let {
        open,
        activeTab,
        hasSelection,
        onTabChange,
        simTabActions,
        sim,
        cell,
    }: {
        open: boolean;
        activeTab: "sim" | "cell";
        hasSelection: boolean;
        onTabChange: (t: "sim" | "cell") => void;
        simTabActions?: Snippet;
        sim?: Snippet;
        cell?: Snippet;
    } = $props();
</script>

{#if open}
    <aside class="rail" aria-label="Configuration panel">
        <!-- Tab bar -->
        <div class="rail__tabs" role="tablist">
            <div class="rail__tab-cell">
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
                {#if simTabActions}
                    <div class="rail__tab-actions">
                        {@render simTabActions()}
                    </div>
                {/if}
            </div>
            <div class="rail__tab-cell">
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
            </div>
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
        top: var(--topbar-h, 3.5rem);
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
        height: 4rem;
        border-bottom: 1px solid var(--color-border-subtle);
        flex-shrink: 0;
    }

    /* Each tab is a column cell: the switch button sits on the top line so the
       "Simulation" / "Cell" labels stay aligned, and the Simulation glyph row
       flows beneath its label. */
    .rail__tab-cell {
        flex: 1;
        display: flex;
        flex-direction: column;
        align-items: center;
        justify-content: flex-start;
        gap: var(--space-1);
        padding-block-start: var(--space-3);
        position: relative;
    }

    .rail__tab {
        display: flex;
        align-items: center;
        justify-content: center;
        gap: var(--space-2);
        border: 0;
        background: transparent;
        cursor: pointer;
        font-family: var(--font-mono);
        font-size: var(--text-xs);
        text-transform: uppercase;
        letter-spacing: var(--tracking-wider);
        color: var(--color-text-muted);
        position: relative;
        transition: color 0.1s;
    }

    .rail__tab-actions {
        display: flex;
        gap: var(--space-2);
    }

    .rail__tab:hover {
        color: var(--color-text);
    }

    .rail__tab--active {
        color: var(--color-text);
        font-weight: 600;
    }

    /* Active underline indicator — anchored to the bottom edge of the tab bar
       (the cell), not the label button, so it clears the glyph row. */
    .rail__tab-cell:has(.rail__tab--active)::after {
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
            opacity: var(--opacity-half);
            transform: scale(0.85);
        }
        100% {
            opacity: 1;
            transform: scale(1);
        }
    }

    /* ── Tab body ── */
    .rail__body {
        flex: 1;
        overflow-y: auto;
    }
</style>
