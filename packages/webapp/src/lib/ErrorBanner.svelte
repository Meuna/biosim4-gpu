<script lang="ts">
    import { fly } from "svelte/transition";
    import { X } from "lucide-svelte";

    let {
        message,
        onClose,
    }: {
        message: string;
        onClose: () => void;
    } = $props();
</script>

<div
    class="error-banner"
    role="alert"
    transition:fly={{ y: 80, duration: 260 }}
>
    <span class="error-banner__msg">{message}</span>
    <button
        class="button button--utility error-banner__close"
        onclick={onClose}
        aria-label="Dismiss error"
    >
        <X size={14} />
    </button>
</div>

<style>
    .error-banner {
        position: fixed;
        bottom: var(--space-4);
        left: 50%;
        transform: translateX(-50%);
        z-index: 90;
        display: flex;
        align-items: center;
        gap: var(--space-3);
        max-width: 32rem;
        padding: var(--space-2) var(--space-2) var(--space-2) var(--space-4);
        background: var(--color-surface-glass);
        border: 1px solid var(--color-warn);
        border-radius: var(--radius-sm);
        color: var(--color-warn);
        font-family: var(--font-mono);
        font-size: var(--text-sm);
        line-height: 1.4;
        backdrop-filter: blur(4px);
    }

    .error-banner__msg {
        word-break: break-word;
    }

    .error-banner__close {
        flex-shrink: 0;
        color: var(--color-warn);
    }
</style>
