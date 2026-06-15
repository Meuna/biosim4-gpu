// Pure grid-zone geometry: maps the viewport, topbar height, and rail state to
// the centred square-ish grid region the canvas and overlays share. Kept free
// of Svelte runes so it is unit-testable in isolation.

export interface GridGeom {
    x: number;
    y: number;
    w: number;
    h: number;
    cx: number;
    cy: number;
}

export interface GridGeomInput {
    viewportW: number;
    viewportH: number;
    topbarH: number;
    railOpen: boolean;
    gridSizeX: number;
    gridSizeY: number;
}

const PAD_TOP = 80;
const PAD_BOTTOM = 180;
const RAIL_W = 380;
const MIN_GRID = 140;
const MAX_GRID = 760;

// Side padding scales with viewport width so small screens give more room to
// the grid. Floor 44px keeps the y-axis end label (GridView: left -2.25rem
// ≈ 36px) on-screen; cap 80px preserves the desktop look. Reaches 80 near
// 1000px wide (continuous — no breakpoint, no resize "pop").
const PAD_SIDE_MIN = 44;
const PAD_SIDE_MAX = 80;
const PAD_SIDE_FRACTION = 0.08;

function sidePadding(viewportW: number): number {
    return Math.min(
        PAD_SIDE_MAX,
        Math.max(PAD_SIDE_MIN, viewportW * PAD_SIDE_FRACTION),
    );
}

export function computeGridGeom(input: GridGeomInput): GridGeom {
    const padSide = sidePadding(input.viewportW);
    const railW = input.railOpen && input.viewportW > 760 ? RAIL_W : 0;
    const availW = input.viewportW - railW - padSide * 2;
    const availH = input.viewportH - input.topbarH - PAD_TOP - PAD_BOTTOM;
    const maxCells = Math.max(input.gridSizeX, input.gridSizeY);
    const maxDim = Math.max(MIN_GRID, Math.min(availW, availH, MAX_GRID));
    const ppc = maxDim / maxCells;
    const w = input.gridSizeX * ppc;
    const h = input.gridSizeY * ppc;
    const x = padSide + (availW - w) / 2;
    const y = input.topbarH + PAD_TOP + (availH - h) / 2;
    return { x, y, w, h, cx: x + w / 2, cy: y + h / 2 };
}
