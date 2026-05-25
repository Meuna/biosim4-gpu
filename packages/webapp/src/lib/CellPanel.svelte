<script lang="ts">
    const HEADINGS = [
        "→ E",
        "↗ NE",
        "↑ N",
        "↖ NW",
        "← W",
        "↙ SW",
        "↓ S",
        "↘ SE",
    ] as const;

    type Agent = {
        id: number;
        gen: number;
        age: number;
        gx: number;
        gy: number;
        heading: number;
    };

    let {
        agent,
        onClear,
    }: {
        agent: Agent | null;
        onClear: () => void;
    } = $props();

    function hexId(id: number): string {
        return id.toString(16).toUpperCase().padStart(4, "0");
    }

    function padId(id: number): string {
        return id.toString().padStart(4, "0");
    }

    /** Brain placeholder — simulated connection coords */
    const CONNECTIONS = [
        [40, 40, 160, 60],
        [40, 40, 160, 100],
        [40, 70, 160, 100],
        [40, 100, 160, 100],
        [40, 100, 160, 140],
        [40, 130, 160, 140],
        [40, 160, 160, 180],
        [40, 190, 160, 180],
        [160, 60, 280, 80],
        [160, 100, 280, 80],
        [160, 100, 280, 120],
        [160, 140, 280, 120],
        [160, 140, 280, 160],
        [160, 180, 280, 160],
    ] as const;

    /** Movement trace polyline points (static placeholder) */
    function buildTrace(): string {
        const pts: string[] = [];
        let x = 4;
        let y = 30;
        for (let i = 0; i < 24; i++) {
            x += 12 + (i % 3) * 2;
            y += Math.sin(i * 0.7) * 6 + (i % 5 === 0 ? -8 : 2);
            y = Math.max(8, Math.min(54, y));
            pts.push(`${x},${y}`);
        }
        return pts.join(" ");
    }

    const TRACE_POINTS = buildTrace();
    const TRACE_LAST = (() => {
        const last = TRACE_POINTS.split(" ").at(-1)!;
        const [lx, ly] = last.split(",");
        return { x: lx, y: ly };
    })();
</script>

{#if !agent}
    <!-- Empty state -->
    <div class="cell-panel cell-panel--empty">
        <p class="small-caps cell-panel__empty-label">No selection</p>
        <p class="cell-panel__empty-display">
            Click any cell<br />
            <em>on the grid.</em>
        </p>
        <p class="cell-panel__empty-hint">
            Hover to preview an agent's identity. Click to pin it here and
            stream its brain, genome, and movement trace.
        </p>
    </div>
{:else}
    <!-- Selection state -->
    <div class="cell-panel">
        <!-- Header -->
        <div class="cell-panel__header-row">
            <p class="small-caps cell-panel__eyebrow">
                <span class="pulse-dot" aria-hidden="true"></span>
                selected agent
            </p>
        </div>
        <div class="cell-panel__title-row">
            <h2 class="cell-panel__title">
                Cell <em>#{padId(agent.id)}</em>
            </h2>
            <button class="cell-panel__deselect" onclick={onClear}>
                ← deselect
            </button>
        </div>
        <p class="cell-panel__sub">
            0x{hexId(agent.id)} · lineage of 8
        </p>

        <!-- Identity section -->
        <div class="section-label" style="margin-top: var(--space-6)">
            <span class="small-caps">Identity</span>
            <span class="cell-panel__hint">gen {agent.gen}</span>
        </div>
        <div class="stat-row">
            <span class="stat-key">generation</span><span class="stat-val"
                >{agent.gen}</span
            >
        </div>
        <div class="stat-row">
            <span class="stat-key">age</span><span class="stat-val"
                >{agent.age} / 300</span
            >
        </div>
        <div class="stat-row">
            <span class="stat-key">lineage</span><span class="stat-val"
                >↩ 8 ancestors</span
            >
        </div>
        <div class="stat-row">
            <span class="stat-key">fitness</span><span class="stat-val"
                >0.72</span
            >
        </div>

        <!-- Position section -->
        <div class="section-label" style="margin-top: var(--space-6)">
            <span class="small-caps">Position</span>
            <span class="cell-panel__hint">t = 384</span>
        </div>
        <div class="stat-row">
            <span class="stat-key">x, y</span><span class="stat-val"
                >{agent.gx}, {agent.gy}</span
            >
        </div>
        <div class="stat-row">
            <span class="stat-key">heading</span><span class="stat-val"
                >{HEADINGS[agent.heading % 8]}</span
            >
        </div>
        <div class="stat-row">
            <span class="stat-key">last action</span><span class="stat-val"
                >MOVE_FW</span
            >
        </div>
        <div class="stat-row">
            <span class="stat-key">osc period</span><span class="stat-val"
                >13 steps</span
            >
        </div>

        <!-- Brain / genome placeholder -->
        <div class="section-label" style="margin-top: var(--space-6)">
            <span class="small-caps">Genome / brain</span>
            <span class="cell-panel__hint">reserved</span>
        </div>
        <!-- TODO: replace with actual brain-graph SVG rendering -->
        <div class="brain-placeholder" aria-label="Brain graph placeholder">
            <svg
                width="100%"
                height="220"
                viewBox="0 0 320 220"
                preserveAspectRatio="xMidYMid meet"
                class="brain-svg"
            >
                <!-- Input neurons -->
                {#each [40, 70, 100, 130, 160, 190] as y, i}
                    <circle
                        cx="40"
                        cy={y}
                        r="5"
                        fill="none"
                        stroke="var(--color-text)"
                        stroke-width="1"
                    />
                    <text
                        x="22"
                        y={y + 3}
                        font-size="8"
                        font-family="var(--font-mono)"
                        fill="var(--color-text-muted)"
                        text-anchor="end">I{i}</text
                    >
                {/each}
                <!-- Hidden neurons -->
                {#each [60, 100, 140, 180] as y}
                    <circle
                        cx="160"
                        cy={y}
                        r="6"
                        fill="var(--color-surface)"
                        stroke="var(--color-text)"
                        stroke-width="1"
                    />
                {/each}
                <!-- Output neurons -->
                {#each [80, 120, 160] as y, i}
                    <circle cx="280" cy={y} r="5" fill="var(--color-text)" />
                    <text
                        x="296"
                        y={y + 3}
                        font-size="8"
                        font-family="var(--font-mono)"
                        fill="var(--color-text-muted)">O{i}</text
                    >
                {/each}
                <!-- Connections -->
                {#each CONNECTIONS as c, i}
                    <line
                        x1={c[0]}
                        y1={c[1]}
                        x2={c[2]}
                        y2={c[3]}
                        stroke="var(--color-text)"
                        stroke-opacity={0.35 + (i % 4) * 0.12}
                        stroke-width="0.8"
                    />
                {/each}
            </svg>
            <p class="brain-placeholder__label small-caps">
                brain · 6 in → 4 hidden → 3 out · 14 conn
            </p>
            <p class="brain-placeholder__expand">⊞ expand</p>
        </div>
        <div class="cell-panel__genome-row">
            <span>genome · 16 genes</span>
            <a href="#genome" class="cell-panel__hex-link">view as hex →</a>
        </div>

        <!-- Movement trace placeholder -->
        <div class="section-label" style="margin-top: var(--space-6)">
            <span class="small-caps">Movement trace</span>
            <span class="cell-panel__hint">last 24 steps</span>
        </div>
        <!-- TODO: replace with actual movement trace rendering -->
        <div class="trace-placeholder" aria-label="Movement trace placeholder">
            <svg
                width="100%"
                height="64"
                viewBox="0 0 320 64"
                preserveAspectRatio="none"
                class="trace-svg"
            >
                <defs>
                    <linearGradient id="traceFade" x1="0" x2="1">
                        <stop
                            offset="0"
                            stop-color="var(--color-text)"
                            stop-opacity="0.15"
                        />
                        <stop
                            offset="1"
                            stop-color="var(--color-text)"
                            stop-opacity="0.9"
                        />
                    </linearGradient>
                </defs>
                <polyline
                    points={TRACE_POINTS}
                    fill="none"
                    stroke="url(#traceFade)"
                    stroke-width="1.2"
                />
                <circle
                    cx={TRACE_LAST.x}
                    cy={TRACE_LAST.y}
                    r="3"
                    fill="var(--color-accent)"
                />
            </svg>
            <span class="trace-placeholder__start">step 200</span>
            <span class="trace-placeholder__end">step 384 ←</span>
        </div>

        <!-- Action buttons -->
        <div class="cell-panel__actions" style="margin-top: var(--space-6)">
            <button
                class="button button--pill button--filled cell-panel__action-primary"
            >
                follow agent
            </button>
            <button class="button button--pill button--ghost">inspect</button>
            <button class="button button--pill button--ghost">clone</button>
        </div>
    </div>
{/if}

<style>
    .cell-panel {
        padding: var(--space-6);
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
        margin: 0 auto;
    }

    /* ── Selection state header ── */
    .cell-panel__header-row {
        display: flex;
        align-items: center;
        gap: var(--space-2);
        margin-bottom: var(--space-2);
    }

    .cell-panel__eyebrow {
        display: flex;
        align-items: center;
        gap: var(--space-2);
        margin: 0;
    }

    .pulse-dot {
        display: inline-block;
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
        white-space: nowrap;
        flex-shrink: 0;
    }

    .cell-panel__title em {
        font-style: italic;
    }

    .cell-panel__deselect {
        border: 0;
        background: transparent;
        cursor: pointer;
        font-family: var(--font-mono);
        font-size: 0.625rem;
        letter-spacing: 0.18em;
        color: var(--color-text-muted);
        text-transform: uppercase;
        padding: var(--space-1) 0;
        flex-shrink: 0;
        transition: color 0.1s;
    }

    .cell-panel__deselect:hover {
        color: var(--color-text);
    }

    .cell-panel__sub {
        font-family: var(--font-mono);
        font-size: 11px;
        color: var(--color-text-muted);
        margin: var(--space-1) 0 0 0;
    }

    .cell-panel__hint {
        font-family: var(--font-mono);
        font-size: 0.625rem;
        color: var(--color-text-muted);
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

    /* ── Brain placeholder ── */
    .brain-placeholder {
        position: relative;
        width: 100%;
        height: 220px;
        border: 1px solid var(--color-border-subtle);
        border-radius: var(--radius-md);
        overflow: hidden;
        background:
            linear-gradient(var(--color-border-subtle) 1px, transparent 1px) 0
                0 / 12px 12px,
            linear-gradient(
                    90deg,
                    var(--color-border-subtle) 1px,
                    transparent 1px
                )
                0 0 / 12px 12px;
    }

    .brain-svg {
        position: absolute;
        inset: 0;
    }

    .brain-placeholder__label {
        position: absolute;
        left: var(--space-2);
        top: var(--space-2);
        margin: 0;
    }

    .brain-placeholder__expand {
        position: absolute;
        right: var(--space-2);
        bottom: var(--space-2);
        font-family: var(--font-mono);
        font-size: 9px;
        color: var(--color-text-muted);
        margin: 0;
    }

    .cell-panel__genome-row {
        display: flex;
        justify-content: space-between;
        margin-top: var(--space-2);
        font-family: var(--font-mono);
        font-size: 0.625rem;
        color: var(--color-text-muted);
    }

    .cell-panel__hex-link {
        color: var(--color-text-muted);
        text-decoration: none;
        transition: color 0.1s;
    }

    .cell-panel__hex-link:hover {
        color: var(--color-text);
    }

    /* ── Movement trace placeholder ── */
    .trace-placeholder {
        position: relative;
    }

    .trace-svg {
        display: block;
    }

    .trace-placeholder__start,
    .trace-placeholder__end {
        position: absolute;
        bottom: 0;
        font-family: var(--font-mono);
        font-size: 9px;
        color: var(--color-text-muted);
    }

    .trace-placeholder__start {
        left: 0;
    }

    .trace-placeholder__end {
        right: 0;
    }

    /* ── Actions ── */
    .cell-panel__actions {
        display: flex;
        gap: var(--space-2);
    }

    .cell-panel__action-primary {
        flex: 1;
        justify-content: center;
    }
</style>
