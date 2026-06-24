// Fade timing for the draft-barrier preview overlay (gh-134). When the user
// edits the barrier list (or grid size), the worker draws the resolved draft
// cells, then fades them out so they do not linger over the live sculpture.
// Pure: no DOM or canvas access, importable from the Web Worker.

// Held fully opaque while the user is likely still editing, then a linear fade
// to transparent. Re-showing resets the elapsed clock, so continuous edits keep
// the overlay up; applying the config clears it outright.
export const PREVIEW_HOLD_MS = 1000;
export const PREVIEW_FADE_MS = 2500;

// Overlay opacity in [0, 1] for a preview that has been visible for `elapsedMs`.
// 1 within the hold window, a linear ramp to 0 across the fade window, and 0
// once fully faded (or for negative elapsed, e.g. clock skew).
export function previewFade(elapsedMs: number): number {
    if (elapsedMs < PREVIEW_HOLD_MS) {
        return 1;
    }
    const faded = (elapsedMs - PREVIEW_HOLD_MS) / PREVIEW_FADE_MS;
    return faded >= 1 ? 0 : 1 - faded;
}
