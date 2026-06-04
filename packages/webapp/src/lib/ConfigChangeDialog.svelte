<script lang="ts">
    import { Undo2, Dna, Trash2 } from "lucide-svelte";

    let {
        open,
        onRevertContinue,
        onPauseSaveGenome,
        onPauseClearGenome,
    }: {
        open: boolean;
        onRevertContinue: () => void;
        onPauseSaveGenome: () => void;
        onPauseClearGenome: () => void;
    } = $props();

    let primaryBtn = $state<HTMLButtonElement | undefined>();

    $effect(() => {
        if (!open) return;

        primaryBtn?.focus();

        function handleKeydown(e: KeyboardEvent): void {
            if (e.key === "Escape") {
                e.preventDefault();
                onRevertContinue();
            }
        }

        window.addEventListener("keydown", handleKeydown);
        return () => window.removeEventListener("keydown", handleKeydown);
    });
</script>

{#if open}
    <div class="dialog-backdrop" aria-hidden="true"></div>
    <div
        class="dialog-panel"
        role="dialog"
        aria-modal="true"
        aria-labelledby="config-change-title"
    >
        <p class="small-caps dialog-eyebrow" id="config-change-title">
            Configuration changed
        </p>
        <p class="dialog-subtitle">Simulation paused.</p>

        <div class="dialog-actions">
            <button
                class="button button--filled dialog-btn"
                bind:this={primaryBtn}
                onclick={onRevertContinue}
            >
                <Undo2 size={14} />
                Revert and continue
            </button>
            <button class="button dialog-btn" onclick={onPauseSaveGenome}>
                <Dna size={14} />
                Pause and save genome
            </button>
            <button
                class="button dialog-btn dialog-btn--warn"
                onclick={onPauseClearGenome}
            >
                <Trash2 size={14} />
                Pause and clear genome
            </button>
        </div>
    </div>
{/if}

<style>
    .dialog-backdrop {
        position: fixed;
        inset: 0;
        z-index: 55;
        background: rgba(10, 10, 10, 0.45);
        backdrop-filter: blur(2px);
    }

    .dialog-panel {
        position: fixed;
        z-index: 60;
        top: 50%;
        left: 50%;
        transform: translate(-50%, -50%);
        background: var(--color-surface);
        border: 1px solid var(--color-border);
        border-radius: var(--radius-md);
        box-shadow: var(--shadow-floating);
        padding: var(--space-6);
        min-width: 22rem;
        display: flex;
        flex-direction: column;
        gap: var(--space-4);
    }

    .dialog-eyebrow {
        margin: 0;
    }

    .dialog-subtitle {
        font-family: var(--font-sans);
        font-size: var(--text-base);
        font-weight: 600;
        color: var(--color-text);
        margin: 0;
        margin-top: calc(-1 * var(--space-2));
    }

    .dialog-actions {
        display: flex;
        flex-direction: column;
        gap: var(--space-2);
    }

    .dialog-btn {
        justify-content: flex-start;
        width: 100%;
        font-size: var(--text-sm);
    }

    .dialog-btn--warn {
        color: var(--color-warn);
        border-color: var(--color-warn);
    }

    .dialog-btn--warn:hover {
        color: var(--color-warn);
        border-color: var(--color-warn);
        opacity: 0.8;
    }
</style>
