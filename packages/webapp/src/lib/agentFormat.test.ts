import { describe, expect, it } from "vitest";

import { HEADINGS } from "./agentFormat";

describe("HEADINGS", () => {
    it("has 8 compass directions", () => {
        expect(HEADINGS).toHaveLength(8);
    });

    it("maps index 0 to east", () => {
        expect(HEADINGS[0]).toBe("→ E");
    });

    it("maps index 4 to west", () => {
        expect(HEADINGS[4]).toBe("← W");
    });
});
