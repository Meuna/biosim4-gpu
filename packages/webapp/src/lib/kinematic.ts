// Pure math helpers for the kinematic sculpture and grid-mode rendering.
// No side effects; no DOM or canvas access. Importable from both the main
// thread and the Web Worker.

/** Canvas-wide kinematic position for agent i at time t (seconds). */
export function kinematicPosition(
    i: number,
    pop: number,
    cx: number,
    cy: number,
    t: number,
    canvasW: number,
    canvasH: number,
): { x: number; y: number; r: number } {
    const theta = pop > 0 ? (i / pop) * Math.PI * 2 : 0;
    // Each agent gets a unique pair of Lissajous frequencies via index modulo
    // small primes, producing varied orbits without any randomness.
    const freqA = 0.25 + (i % 11) * 0.03;
    const freqB = 0.18 + (i % 7) * 0.04;
    // Radii exceed half the canvas so agents orbit beyond the viewport edges.
    const rX = canvasW * 0.7;
    const rY = canvasH * 0.7;
    return {
        x: cx + rX * Math.cos(theta + t * freqA),
        y: cy + rY * Math.sin(theta + t * freqB),
        // Radius pulses between 1 and 2 px.
        r: 1.5 + Math.sin(t * 2.1 + theta * 3) * 0.5,
    };
}

/** Canvas-pixel centre + radius for a simulation grid cell (gx, gy).
 *  Supports rectangular grids: gridW/gridH are the pixel dimensions of the
 *  grid region on the canvas; gridCellsX/Y are the simulation cell counts. */
export function gridPosition(
    gx: number,
    gy: number,
    gridX: number,
    gridY: number,
    gridW: number,
    gridH: number,
    gridCellsX: number,
    gridCellsY: number,
): { x: number; y: number; r: number } {
    const cellPxW = gridW / gridCellsX;
    const cellPxH = gridH / gridCellsY;
    return {
        x: gridX + (gx + 0.5) * cellPxW,
        y: gridY + (gy + 0.5) * cellPxH,
        r: Math.max(1.5, Math.min(cellPxW, cellPxH) * 0.4),
    };
}

/** Linear interpolation between two 2-D points. */
export function lerpVec2(
    a: { x: number; y: number },
    b: { x: number; y: number },
    t: number,
): { x: number; y: number } {
    return { x: a.x + (b.x - a.x) * t, y: a.y + (b.y - a.y) * t };
}

/** Smooth-step (quadratic ease-in-out), t ∈ [0, 1]. */
export function easeInOut(t: number): number {
    return t < 0.5 ? 2 * t * t : -1 + (4 - 2 * t) * t;
}
