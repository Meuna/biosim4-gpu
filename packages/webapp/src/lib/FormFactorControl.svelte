<script lang="ts">
    // FormFactorControl — the form-factor preset half of a simulation preset.
    // Three quick-pick pills (desktop / tablet / phone) that each set population
    // and grid size in one click, styled exactly like the GridSizeControl pills.
    // The active pill is derived by the parent via activeFormFactor(); a manual
    // population/grid edit matches no form factor, so all pills go inactive.
    import { Monitor, TabletSmartphone, Smartphone } from "lucide-svelte";
    import type { FormFactor } from "./formFactor";

    interface Props {
        value: FormFactor | null;
        disabled?: boolean;
        onchange: (ff: FormFactor) => void;
    }
    const { value, disabled = false, onchange }: Props = $props();

    const OPTIONS = [
        { ff: "desktop", label: "Desktop", icon: Monitor },
        { ff: "tablet", label: "Tablet", icon: TabletSmartphone },
        { ff: "phone", label: "Phone", icon: Smartphone },
    ] as const;
</script>

<!-- Form-factor pills — clicking one sets population + grid for that device -->
<div class="form-factor__pills">
    {#each OPTIONS as o}
        <button
            class="button button--pill {value === o.ff
                ? 'button--filled'
                : 'button--ghost'}"
            {disabled}
            onclick={() => onchange(o.ff)}
            aria-pressed={value === o.ff}
            aria-label={o.label}
            title={o.label}
        >
            <o.icon size={16} />
        </button>
    {/each}
</div>

<style>
    .form-factor__pills {
        display: flex;
        gap: var(--space-3);
    }
</style>
