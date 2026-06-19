import { describe, it, expect } from "vitest";
import { shouldSpaceBeat } from "./beatTrigger";

function el(tag: string, attrs: Record<string, string> = {}): HTMLElement {
    const node = document.createElement(tag);
    for (const [k, v] of Object.entries(attrs)) node.setAttribute(k, v);
    return node;
}

describe("shouldSpaceBeat", () => {
    it("fires on space in kinematic mode over a plain target", () => {
        expect(shouldSpaceBeat(" ", "kinematic", el("div"))).toBe(true);
    });

    it("fires on space in kinematic mode with no target", () => {
        expect(shouldSpaceBeat(" ", "kinematic", null)).toBe(true);
    });

    it("ignores non-space keys", () => {
        expect(shouldSpaceBeat("Enter", "kinematic", el("div"))).toBe(false);
        expect(shouldSpaceBeat("a", "kinematic", el("div"))).toBe(false);
    });

    it("does not intercept space in grid mode", () => {
        expect(shouldSpaceBeat(" ", "grid", el("div"))).toBe(false);
    });

    it("does not fire when focus is on an editable or interactive control", () => {
        expect(shouldSpaceBeat(" ", "kinematic", el("input"))).toBe(false);
        expect(shouldSpaceBeat(" ", "kinematic", el("textarea"))).toBe(false);
        expect(shouldSpaceBeat(" ", "kinematic", el("select"))).toBe(false);
        expect(shouldSpaceBeat(" ", "kinematic", el("button"))).toBe(false);
        expect(shouldSpaceBeat(" ", "kinematic", el("a"))).toBe(false);
    });

    it("does not fire over the brand span (role=button) or contenteditable", () => {
        expect(
            shouldSpaceBeat(" ", "kinematic", el("span", { role: "button" })),
        ).toBe(false);
        expect(
            shouldSpaceBeat(
                " ",
                "kinematic",
                el("div", { contenteditable: "true" }),
            ),
        ).toBe(false);
    });
});
