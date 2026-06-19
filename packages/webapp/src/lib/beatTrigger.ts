// Pure predicate for the space-key beat trigger (issue #80, sub-plan C).
//
// Space pulses the idle sculpture full-viewport. The decision is kept pure so
// the two load-bearing gates can be unit-tested without the worker/canvas
// coupling: it must fire ONLY while the sculpture is showing and focus is not on
// an editable or interactive control. Keeping space inert in grid mode is what
// lets the caller safely preventDefault without breaking space-activates-button.

/** Tag/role names that own the space key (typing or activation) — never beat. */
function isInteractiveTarget(target: EventTarget | null): boolean {
    if (!(target instanceof HTMLElement)) return false;
    // `isContentEditable` covers inherited editability in real browsers; the
    // explicit attribute also catches it where the property is unimplemented.
    const ce = target.getAttribute("contenteditable");
    if (target.isContentEditable || ce === "" || ce === "true") return true;
    if (target.getAttribute("role") === "button") return true;
    return ["INPUT", "TEXTAREA", "SELECT", "BUTTON", "A"].includes(
        target.tagName,
    );
}

/**
 * True only when a space keypress should fire a full-viewport beat: the key is
 * space, the sculpture is showing (`"kinematic"`), and focus is not on an
 * editable/interactive control. In grid mode (or over a control) it returns
 * false so the caller leaves the event untouched.
 */
export function shouldSpaceBeat(
    key: string,
    renderMode: "kinematic" | "grid",
    target: EventTarget | null,
): boolean {
    if (key !== " ") return false;
    if (renderMode !== "kinematic") return false;
    return !isInteractiveTarget(target);
}
