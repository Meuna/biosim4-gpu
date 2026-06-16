<script lang="ts">
    let {
        message,
    }: {
        message: string;
    } = $props();

    function reload(): void {
        window.location.reload();
    }
</script>

<div
    class="fatal-overlay"
    role="alertdialog"
    aria-modal="true"
    aria-label="Simulation failed to start"
>
    <div class="fatal-overlay__card">
        <p class="fatal-overlay__title">Simulation failed to start</p>
        <p class="fatal-overlay__msg">{message}</p>
        <button class="button button--filled" onclick={reload}>Reload</button>
    </div>
</div>

<style>
    .fatal-overlay {
        position: fixed;
        inset: 0;
        z-index: 200;
        display: flex;
        align-items: center;
        justify-content: center;
        background: color-mix(in srgb, var(--color-surface) 80%, transparent);
        backdrop-filter: blur(2px);
    }

    .fatal-overlay__card {
        display: flex;
        flex-direction: column;
        align-items: center;
        gap: var(--space-4);
        max-width: 32rem;
        padding: var(--space-6) var(--space-8);
        background: var(--color-surface-glass);
        border: 1px solid var(--color-warn);
        border-radius: var(--radius-sm);
        text-align: center;
    }

    .fatal-overlay__title {
        font-family: var(--font-mono);
        font-size: var(--text-base);
        font-weight: 700;
        color: var(--color-warn);
        margin: 0;
        letter-spacing: var(--tracking-tight);
    }

    .fatal-overlay__msg {
        font-family: var(--font-mono);
        font-size: var(--text-sm);
        color: var(--color-text);
        line-height: 1.4;
        word-break: break-word;
        margin: 0;
    }
</style>
