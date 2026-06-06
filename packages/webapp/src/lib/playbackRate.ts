export function stepDelay(targetFps: number, elapsedMs: number): number {
    if (targetFps <= 0) return 0;
    return Math.max(0, 1000 / targetFps - elapsedMs);
}

export function createFpsWindow(windowMs = 1000): {
    tick(now: number): number | null;
} {
    let start: number | null = null;
    let steps = 0;

    return {
        tick(now: number): number | null {
            if (start === null) {
                start = now;
                steps = 0;
                return null;
            }
            steps++;
            const elapsed = now - start;
            if (elapsed >= windowMs) {
                const fps = steps / (elapsed / 1000);
                start = now;
                steps = 0;
                return fps;
            }
            return null;
        },
    };
}
