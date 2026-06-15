<script lang="ts">
    let {
        open,
        disabled = false,
        onConfirm,
        onCancel,
        confirmLabel = "Yes",
        cancelLabel = "No",
    }: {
        open: boolean;
        disabled?: boolean;
        onConfirm: () => void;
        onCancel: () => void;
        confirmLabel?: string;
        cancelLabel?: string;
    } = $props();
</script>

{#if open}
    <div class="confirm-inline" role="group" aria-label="Confirm action">
        <button
            class="button button--pill confirm-inline__btn--confirm"
            {disabled}
            onclick={onConfirm}
        >
            {confirmLabel}
        </button>
        <button
            class="button button--pill button--ghost"
            {disabled}
            onclick={onCancel}
        >
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

    .confirm-inline__btn--confirm {
        color: var(--color-warn);
        border-color: var(--color-warn);
    }

    .confirm-inline__btn--confirm:hover {
        color: var(--color-warn);
        border-color: var(--color-warn);
        opacity: var(--opacity-strong);
    }
</style>
