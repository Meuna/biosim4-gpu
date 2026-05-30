import { mulberry32 } from "./prng";

describe("mulberry32", () => {
    it("produces values in [0, 1)", () => {
        const next = mulberry32(12345);
        for (let i = 0; i < 1000; i++) {
            const v = next();
            expect(v).toBeGreaterThanOrEqual(0);
            expect(v).toBeLessThan(1);
        }
    });

    it("is deterministic for a given seed", () => {
        const a = mulberry32(42);
        const b = mulberry32(42);
        const seqA = Array.from({ length: 50 }, () => a());
        const seqB = Array.from({ length: 50 }, () => b());
        expect(seqA).toEqual(seqB);
    });

    it("yields different sequences for different seeds", () => {
        const a = mulberry32(1);
        const b = mulberry32(2);
        expect(a()).not.toEqual(b());
    });
});
