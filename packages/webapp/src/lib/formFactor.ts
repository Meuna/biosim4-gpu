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

// Population + grid overrides per handheld form factor. Desktop uses DEFAULTS
// unchanged. Only these three params differ; everything else stays at DEFAULTS
// by construction (see configForFormFactor).
const OVERRIDES: Record<
    Exclude<FormFactor, "desktop">,
    Pick<SimParams, "population" | "gridSizeX" | "gridSizeY">
> = {
    tablet: { population: 1500, gridSizeX: 96, gridSizeY: 96 },
    phone: { population: 700, gridSizeX: 64, gridSizeY: 64 },
};

// Builds the initial simulation config for a form factor by layering its
// overrides on top of DEFAULTS. structuredClone keeps the returned object's
// nested challenge/barriers from aliasing the shared DEFAULTS constant.
export function configForFormFactor(ff: FormFactor): SimParams {
    const base = structuredClone(DEFAULTS);
    return ff === "desktop" ? base : { ...base, ...OVERRIDES[ff] };
}
