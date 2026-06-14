import { snapshotCompatNote } from "./version";

describe("snapshotCompatNote", () => {
    it("returns null when compatible", () => {
        expect(snapshotCompatNote("v0.1.0", "compatible")).toBeNull();
    });

    it("returns null when there is no release to compare against", () => {
        expect(snapshotCompatNote("7d98057", "unreleased")).toBeNull();
    });

    it("names the release and flags incompatibility when diverged", () => {
        const note = snapshotCompatNote("v0.1.0", "diverged");
        expect(note).toContain("v0.1.0");
        expect(note).toMatch(/incompatible/i);
    });
});
