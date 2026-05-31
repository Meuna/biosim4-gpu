<script lang="ts">
    import { Maximize2, SearchX, Shuffle, Skull } from "lucide-svelte";
    import type { AgentInfo } from "../workers/sim.worker";
    import type { BrainConn } from "./brain";
    import { HEADINGS } from "./agentFormat";
    import BrainExplorer from "./BrainExplorer.svelte";

    let {
        agent,
        brain,
        isSelected,
        onClear,
        onNavigate,
        onShuffle,
        onSelectById,
        onExpandBrain,
    }: {
        agent: AgentInfo | null;
        brain: { conns: BrainConn[]; neuronCount: number } | null;
        isSelected: boolean;
        onClear: () => void;
        onNavigate: (dir: -1 | 1) => void;
        onShuffle: () => void;
        onSelectById: (id: number) => void;
        onExpandBrain: () => void;
    } = $props();

    function padId(id: number): string {
        return id.toString().padStart(4, "0");
    }

    let editingId = $state(false);
    let editIdVal = $state("");
    let editIdInput = $state<HTMLInputElement | undefined>();

    $effect(() => {
        if (editingId && editIdInput) editIdInput.select();
    });

    $effect(() => {
        // eslint-disable-next-line @typescript-eslint/no-unused-expressions
        agent?.id;
        editingId = false;
    });

    function startIdEdit() {
        editIdVal = agent!.id.toString();
        editingId = true;
    }

    function commitIdEdit() {
        const n = parseInt(editIdVal, 10);
        if (!isNaN(n) && n >= 0) onSelectById(n);
        editingId = false;
    }

    function cancelIdEdit() {
        editingId = false;
    }
</script>

{#if !agent}
    <!-- Empty state -->
    <div class="cell-panel cell-panel--empty">
        <p class="small-caps cell-panel__empty-label">No selection</p>
        <p class="cell-panel__empty-display">
            Ctrl+hover or click<br />
            <em>an agent.</em>
        </p>
        <p class="cell-panel__empty-hint">
            Click to pin; Ctrl+hover to preview.
        </p>
        <button class="cell-panel__shuffle-btn" onclick={onShuffle}>
            random agent
        </button>
    </div>
{:else}
    <!-- Selection state -->
    <div class="cell-panel" class:cell-panel--dead={!agent.alive}>
        <!-- Header -->
        {#if isSelected}
            <div class="cell-panel__nav-row">
                <h2 class="cell-panel__title">
                    Agent
                    {#if editingId}
                        <input
                            type="number"
                            min="0"
                            bind:value={editIdVal}
                            bind:this={editIdInput}
                            onblur={commitIdEdit}
                            onkeydown={(e) => {
                                if (e.key === "Enter") {
                                    e.preventDefault();
                                    commitIdEdit();
                                }
                                if (e.key === "Escape") cancelIdEdit();
                            }}
                            class="cell-panel__id-input"
                            aria-label="Jump to agent ID"
                        />
                    {:else}
                        <button
                            class="cell-panel__id-btn"
                            onclick={startIdEdit}
                            title="Click to jump to agent by ID"
                            ><em>#{padId(agent.id)}</em></button
                        >
                    {/if}
                    {#if !agent.alive}
                        <Skull size={20} aria-label="Agent deceased" />
                    {/if}
                </h2>
                <button
                    class="cell-panel__nav-btn"
                    onclick={() => onNavigate(-1)}
                    aria-label="Previous agent"
                >
                    ←
                </button>
                <button
                    class="cell-panel__nav-btn"
                    onclick={() => onNavigate(1)}
                    aria-label="Next agent"
                >
                    →
                </button>
                <button
                    class="cell-panel__nav-btn"
                    onclick={onShuffle}
                    aria-label="Random agent"
                >
                    <Shuffle size={13} />
                </button>
                <button
                    class="cell-panel__nav-btn"
                    onclick={onClear}
                    aria-label="Deselect agent"
                >
                    <SearchX size={13} />
                </button>
            </div>
        {:else}
            <div class="cell-panel__title-row">
                <h2 class="cell-panel__title">
                    Agent <em>#{padId(agent.id)}</em>
                    {#if !agent.alive}
                        <Skull size={20} aria-label="Agent deceased" />
                    {/if}
                </h2>
                <span class="cell-panel__sub">click to pin</span>
            </div>
        {/if}

        <!-- Identity section -->
        <div class="section-label" style="margin-top: var(--space-6)">
            <span class="small-caps">Identity</span>
        </div>
        <div class="stat-row">
            <span class="stat-key">fingerprint</span>
            <span class="stat-val">0x{agent.fingerprint}</span>
        </div>
        <div class="stat-row">
            <span class="stat-key">challenge bits</span>
            <span class="stat-val"
                >0x{agent.challengeBits.toString(16).padStart(8, "0")}</span
            >
        </div>

        <!-- Position section -->
        <div class="section-label" style="margin-top: var(--space-6)">
            <span class="small-caps">Position</span>
        </div>
        <div class="stat-row">
            <span class="stat-key">x, y</span>
            <span class="stat-val">{agent.gx}, {agent.gy}</span>
        </div>
        <div class="stat-row">
            <span class="stat-key">born at</span>
            <span class="stat-val">{agent.birthX}, {agent.birthY}</span>
        </div>
        <div class="stat-row">
            <span class="stat-key">heading</span>
            <span class="stat-val">{HEADINGS[agent.heading % 8]}</span>
        </div>

        <!-- Behaviour section -->
        <div class="section-label" style="margin-top: var(--space-6)">
            <span class="small-caps">Behaviour</span>
        </div>
        <div class="stat-row">
            <span class="stat-key">osc period</span>
            <span class="stat-val">{agent.oscPeriod} steps</span>
        </div>
        <div class="stat-row">
            <span class="stat-key">responsiveness</span>
            <span class="stat-val">{agent.responsiveness.toFixed(2)}</span>
        </div>
        <div class="stat-row">
            <span class="stat-key">LOS range</span>
            <span class="stat-val">{agent.losRange}</span>
        </div>

        <!-- Brain network -->
        <div class="section-label" style="margin-top: var(--space-6)">
            <span class="small-caps">Genome / brain</span>
            {#if brain}
                <button
                    class="cell-panel__expand-btn"
                    onclick={onExpandBrain}
                    aria-label="Expand brain explorer"
                    title="Expand brain explorer"
                >
                    <Maximize2 size={13} />
                </button>
            {/if}
        </div>
        {#if brain}
            <BrainExplorer
                conns={brain.conns}
                neuronCount={brain.neuronCount}
                variant="preview"
            />
        {:else}
            <p class="cell-panel__no-brain">
                Brain unavailable for this agent.
            </p>
        {/if}
    </div>
{/if}

<style>
    .cell-panel {
        padding: var(--space-6);
    }

    .cell-panel--dead {
        background: color-mix(in srgb, var(--color-surface) 88%, red 12%);
    }

    /* ── Empty state ── */
    .cell-panel--empty {
        padding: var(--space-16) var(--space-6);
        text-align: center;
    }

    .cell-panel__empty-label {
        margin: 0 0 var(--space-3) 0;
    }

    .cell-panel__empty-display {
        font-family: var(--font-sans);
        font-size: var(--text-2xl);
        font-weight: 700;
        line-height: 1.18;
        color: var(--color-text-muted);
        margin: 0 0 var(--space-6) 0;
    }

    .cell-panel__empty-display em {
        font-style: italic;
    }

    .cell-panel__empty-hint {
        font-family: var(--font-sans);
        font-size: 0.75rem;
        color: var(--color-text-muted);
        max-width: 15rem;
        margin: 0 auto var(--space-4) auto;
    }

    .cell-panel__shuffle-btn {
        border: 1px solid var(--color-border);
        background: transparent;
        cursor: pointer;
        font-family: var(--font-mono);
        font-size: 0.625rem;
        letter-spacing: 0.12em;
        color: var(--color-text-muted);
        text-transform: uppercase;
        padding: var(--space-2) var(--space-4);
        border-radius: var(--radius-sm);
        transition:
            color 0.1s,
            border-color 0.1s;
    }

    .cell-panel__shuffle-btn:hover {
        color: var(--color-text);
        border-color: var(--color-text);
    }

    /* ── Nav row (pinned header) ── */
    .cell-panel__nav-row {
        display: flex;
        align-items: center;
        gap: var(--space-2);
        margin-bottom: var(--space-1);
    }

    .cell-panel__nav-btn {
        border: 0;
        background: transparent;
        cursor: pointer;
        font-family: var(--font-mono);
        font-size: 0.75rem;
        color: var(--color-text-muted);
        padding: var(--space-1);
        flex-shrink: 0;
        transition: color 0.1s;
    }

    .cell-panel__nav-btn:hover {
        color: var(--color-text);
    }

    .cell-panel__title-row {
        display: flex;
        align-items: baseline;
        justify-content: space-between;
        gap: var(--space-3);
    }

    .cell-panel__title {
        font-family: var(--font-sans);
        font-size: var(--text-2xl);
        font-weight: 700;
        line-height: 1.15;
        color: var(--color-text);
        margin: 0;
        display: flex;
        align-items: center;
        gap: var(--space-2);
        flex: 1;
        min-width: 0;
    }

    .cell-panel__title em {
        font-style: italic;
    }

    .cell-panel__sub {
        font-family: var(--font-mono);
        font-size: 11px;
        color: var(--color-text-muted);
        margin: var(--space-1) 0 0 0;
    }

    /* ── Stat rows ── */
    .stat-row {
        display: flex;
        justify-content: space-between;
        padding: var(--space-1) 0;
        border-bottom: 1px dotted var(--color-border-subtle);
    }

    .stat-key {
        font-family: var(--font-sans);
        font-size: 0.75rem;
        color: var(--color-text-muted);
    }

    .stat-val {
        font-family: var(--font-mono);
        font-size: 0.75rem;
        color: var(--color-text);
    }

    /* ── Brain network ── */
    .cell-panel__expand-btn {
        border: 0;
        background: transparent;
        cursor: pointer;
        color: var(--color-text-muted);
        padding: var(--space-1);
        display: inline-flex;
        transition: color 0.1s;
    }

    .cell-panel__expand-btn:hover {
        color: var(--color-text);
    }

    .cell-panel__no-brain {
        font-family: var(--font-mono);
        font-size: 0.75rem;
        color: var(--color-text-muted);
        margin: var(--space-2) 0 0 0;
    }

    /* ── Agent ID click-to-edit ── */
    .cell-panel__id-btn {
        border: 0;
        background: transparent;
        cursor: text;
        font: inherit;
        color: inherit;
        padding: 0;
    }

    .cell-panel__id-btn em {
        font-style: italic;
    }

    .cell-panel__id-btn:hover em {
        text-decoration: underline dotted;
    }

    .cell-panel__id-input {
        font-family: var(--font-mono);
        font-size: var(--text-2xl);
        font-weight: 700;
        width: 5ch;
        border: 0;
        border-bottom: 1px solid var(--color-border);
        background: transparent;
        color: var(--color-text);
        padding: 0;
        appearance: textfield;
    }

    .cell-panel__id-input::-webkit-inner-spin-button,
    .cell-panel__id-input::-webkit-outer-spin-button {
        display: none;
    }
</style>
