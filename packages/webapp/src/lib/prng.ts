// Deterministic, seedable pseudo-random number generator. Shared by the brain
// fixtures and the layout relaxation so that "same input → same output" holds
// across runs. Never use Math.random / Date.now / performance.now where
// determinism is required — use this instead.

/**
 * mulberry32 — a tiny, fast 32-bit PRNG. Given the same seed it always yields
 * the same sequence. Returns a function producing floats in [0, 1).
 */
export function mulberry32(seed: number): () => number {
    let a = seed >>> 0;
    return function next(): number {
        a |= 0;
        a = (a + 0x6d2b79f5) | 0;
        let t = Math.imul(a ^ (a >>> 15), 1 | a);
        t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
        return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
}
