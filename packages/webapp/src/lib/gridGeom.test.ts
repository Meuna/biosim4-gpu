import {
    computeGridGeom,
    hamburgerInset,
    type GridGeomInput,
} from "./gridGeom";

const base: GridGeomInput = {
    viewportW: 1440,
    viewportH: 900,
    topbarH: 56,
    railOpen: false,
    gridSizeX: 128,
    gridSizeY: 128,
};

describe("computeGridGeom", () => {
    it("gives a small form factor (393px) most of the width", () => {
        // iPhone 14: 393×852, topbar wrapped to 3 rows (~120px), rail closed.
        const g = computeGridGeom({
            ...base,
            viewportW: 393,
            viewportH: 852,
            topbarH: 120,
        });
        // Side padding floors at 44px → grid spans ~78% of the viewport width,
        // up from ~60% with the old fixed 80px padding.
        expect(g.w).toBeGreaterThanOrEqual(0.76 * 393);
        // Left gap must stay ≥36px so the y-axis end label is not clipped.
        expect(g.x).toBeGreaterThanOrEqual(36);
    });

    it("is unchanged on desktop (side padding caps at 80px)", () => {
        // 1440×900, topbar 56px: availW=1280, availH=584 → height-limited at 584.
        const g = computeGridGeom(base);
        expect(g.w).toBe(584);
        expect(g.h).toBe(584);
        expect(g.x).toBe(80 + (1280 - 584) / 2);
        expect(g.y).toBe(136);
    });

    it("subtracts the rail width only above 760px", () => {
        // At 1000px wide the grid is width-limited, so the open rail shrinks it.
        const closed = computeGridGeom({ ...base, viewportW: 1000 });
        const open = computeGridGeom({
            ...base,
            viewportW: 1000,
            railOpen: true,
        });
        expect(open.w).toBeLessThan(closed.w);

        // At exactly 760px the rail is a full-width overlay, so it is ignored.
        const narrowClosed = computeGridGeom({
            ...base,
            viewportW: 760,
            viewportH: 1200,
        });
        const narrowOpen = computeGridGeom({
            ...base,
            viewportW: 760,
            viewportH: 1200,
            railOpen: true,
        });
        expect(narrowOpen).toEqual(narrowClosed);
    });

    it("clamps the grid between 140 and 760 pixels", () => {
        const tiny = computeGridGeom({
            ...base,
            viewportW: 100,
            viewportH: 100,
        });
        expect(tiny.w).toBe(140);

        const huge = computeGridGeom({
            ...base,
            viewportW: 5000,
            viewportH: 5000,
        });
        expect(huge.w).toBe(760);
    });
});

describe("hamburgerInset", () => {
    it("hugs the border on a small viewport (375px)", () => {
        // iPhone 12 mini: the button tucks in close to the right edge so the
        // grid-hint text clears it.
        const inset = hamburgerInset(375);
        expect(inset).toBeGreaterThanOrEqual(8);
        expect(inset).toBeLessThan(24);
    });

    it("caps at the desktop look (24px) once wide enough", () => {
        expect(hamburgerInset(1000)).toBe(24);
        expect(hamburgerInset(1440)).toBe(24);
    });

    it("floors at 8px on a very narrow viewport", () => {
        expect(hamburgerInset(100)).toBe(8);
    });

    it("shrinks monotonically as the viewport narrows", () => {
        const wide = hamburgerInset(900);
        const mid = hamburgerInset(500);
        const narrow = hamburgerInset(375);
        expect(wide).toBeGreaterThanOrEqual(mid);
        expect(mid).toBeGreaterThanOrEqual(narrow);
    });
});
