import type { SimParams } from "../workers/sim.worker";
import { DEFAULTS } from "./tomlConfig";

// Device form factor, chosen once at app startup to seed the initial simulation
// config. The desktop default (high population, large grid) is too heavy for
// handheld devices, so tablet and phone start with lighter populations and
// grids. See configForFormFactor for the concrete values.
export type FormFactor = "desktop" | "tablet" | "phone";

// Viewport width (px) below which a handheld device is treated as a phone
// rather than a tablet. Matches the handheld breakpoint used by gridGeom.ts
// and RightRail (the rail goes full-width below this width).
const HANDHELD_PHONE_MAX = 760;

// Pure form-factor classifier, split out from the DOM so it is unit-testable.
//
// A device with a hover-capable fine pointer (a real mouse) is a desktop —
// even a narrow window, and even a touch-capable laptop whose *primary* pointer
// is still the mouse. Only when there is no such pointer do we treat it as a
// handheld device, and split tablet vs phone by viewport width. Keying on the
// pointer trait first (rather than viewport alone) is what lets us discriminate
// a desktop user from a handheld one without relying on screen size.
export function classifyFormFactor(env: {
    hoverFinePointer: boolean;
    viewportW: number;
}): FormFactor {
    if (env.hoverFinePointer) return "desktop";
    return env.viewportW < HANDHELD_PHONE_MAX ? "phone" : "tablet";
}

// Reads the live environment (media query + viewport) and classifies it.
export function detectFormFactor(): FormFactor {
    return classifyFormFactor({
        hoverFinePointer: window.matchMedia(
            "(hover: hover) and (pointer: fine)",
        ).matches,
        viewportW: window.innerWidth,
    });
}

// Population + grid size per form factor — the orthogonal "form factor preset"
// half of a simulation preset. The desktop values match DEFAULTS; tablet and
// phone start lighter. These three params are the *only* ones a form factor
// owns; a named preset owns everything else.
const OVERRIDES: Record<
    FormFactor,
    Pick<SimParams, "population" | "gridSizeX" | "gridSizeY">
> = {
    desktop: { population: 3000, gridSizeX: 128, gridSizeY: 128 },
    tablet: { population: 1500, gridSizeX: 96, gridSizeY: 96 },
    phone: { population: 700, gridSizeX: 64, gridSizeY: 64 },
};

// Layers a form factor's population + grid overrides on top of a base config.
// A shallow spread is enough — a form factor only owns the three top-level
// scalars — and it matches the panel's onDraftChange idiom, so this works on a
// Svelte $state draft proxy (which structuredClone cannot clone). This is the
// composition primitive: a named preset supplies the base, the form factor
// supplies pop/grid.
export function applyFormFactor(base: SimParams, ff: FormFactor): SimParams {
    return { ...base, ...OVERRIDES[ff] };
}

// Builds the initial simulation config for a form factor from DEFAULTS — the
// startup seed before any named preset is chosen. Clones DEFAULTS first so the
// returned object's nested challenge/barriers do not alias the shared constant.
export function configForFormFactor(ff: FormFactor): SimParams {
    return applyFormFactor(structuredClone(DEFAULTS), ff);
}

// The form factor whose pop/grid exactly match the given config, or null when
// none do (the user hand-edited population or grid). Drives the form-factor
// pill's active state, mirroring how the grid pills deactivate on manual edits.
export function activeFormFactor(p: SimParams): FormFactor | null {
    for (const ff of ["desktop", "tablet", "phone"] as const) {
        const o = OVERRIDES[ff];
        if (
            p.population === o.population &&
            p.gridSizeX === o.gridSizeX &&
            p.gridSizeY === o.gridSizeY
        ) {
            return ff;
        }
    }
    return null;
}
