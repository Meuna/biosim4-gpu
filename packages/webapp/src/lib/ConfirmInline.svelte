<script lang="ts">
    let {
        open,
        onConfirm,
        onCancel,
        confirmLabel = "Yes",
        cancelLabel = "No",
    }: {
        open: boolean;
        onConfirm: () => void;
        onCancel: () => void;
        confirmLabel?: string;
        cancelLabel?: string;
    } = $props();
</script>

{#if open}
    <div class="confirm-inline" role="group" aria-label="Confirm action">
        <button
            class="confirm-inline__btn confirm-inline__btn--confirm"
            onclick={onConfirm}
        >
            {confirmLabel}
        </button>
        <button class="confirm-inline__btn" onclick={onCancel}>
            {cancelLabel}
        </button>
    </div>
{/if}

<style>
    .confirm-inline {
        display: flex;
        align-items: center;
        gap: var(--space-1);
    }

    .confirm-inline__btn {
        display: inline-flex;
        align-items: center;
        height: 2rem;
        padding: 0 var(--space-2);
        background: transparent;
        border: 1px solid var(--color-border-subtle);
        border-radius: var(--radius-sm);
        cursor: pointer;
        font-family: var(--font-mono);
        font-size: 0.75rem;
        color: var(--color-text-muted);
        letter-spacing: 0.04em;
        white-space: nowrap;
        transition:
            color 0.1s,
            border-color 0.1s;
    }

    .confirm-inline__btn:hover {
        color: var(--color-text);
        border-color: var(--color-border);
    }

    .confirm-inline__btn:focus-visible {
        outline: 2px solid var(--color-accent);
        outline-offset: 2px;
    }

    .confirm-inline__btn--confirm {
        color: var(--color-warn);
        border-color: var(--color-warn);
    }

    .confirm-inline__btn--confirm:hover {
        color: var(--color-warn);
        border-color: var(--color-warn);
        opacity: 0.8;
    }
</style>
