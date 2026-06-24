import { describe, expect, it } from "vitest";
import {
    PREVIEW_FADE_MS,
    PREVIEW_HOLD_MS,
    previewFade,
} from "./barrierPreview";

describe("previewFade", () => {
    it("is fully opaque during the hold window", () => {
        expect(previewFade(0)).toBe(1);
        expect(previewFade(PREVIEW_HOLD_MS - 1)).toBe(1);
    });

    it("clamps negative elapsed to fully opaque", () => {
        expect(previewFade(-500)).toBe(1);
    });

    it("ramps linearly to zero across the fade window", () => {
        expect(previewFade(PREVIEW_HOLD_MS)).toBeCloseTo(1);
        expect(previewFade(PREVIEW_HOLD_MS + PREVIEW_FADE_MS / 2)).toBeCloseTo(
            0.5,
        );
    });

    it("is fully transparent once faded out", () => {
        expect(previewFade(PREVIEW_HOLD_MS + PREVIEW_FADE_MS)).toBe(0);
        expect(previewFade(PREVIEW_HOLD_MS + PREVIEW_FADE_MS + 10_000)).toBe(0);
    });
});
