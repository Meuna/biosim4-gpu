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
            class="button button--pill confirm-inline__btn--confirm"
            onclick={onConfirm}
        >
            {confirmLabel}
        </button>
        <button class="button button--pill button--ghost" onclick={onCancel}>
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
        opacity: 0.8;
    }
</style>
