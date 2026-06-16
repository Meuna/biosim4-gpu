// Vitest setup — jsdom polyfills for browser APIs the components rely on.

// Svelte's `bind:clientWidth`/`bind:clientHeight` use a ResizeObserver, which
// jsdom does not implement. A no-op stub is enough: layout is 0×0 under jsdom,
// and components guard against that.
class ResizeObserverStub {
    observe(): void {}
    unobserve(): void {}
    disconnect(): void {}
}

globalThis.ResizeObserver ??=
    ResizeObserverStub as unknown as typeof ResizeObserver;

// Svelte 5 drives transitions (e.g. `transition:fly`) through the Web Animations
// API, which jsdom does not implement. A stub that resolves immediately is
// enough: tests assert on rendered output, not animation frames.
Element.prototype.animate ??= function animate(this: Element) {
    return {
        cancel() {},
        finish() {},
        play() {},
        pause() {},
        onfinish: null,
        finished: Promise.resolve(),
        currentTime: 0,
        startTime: 0,
        playState: "finished",
    } as unknown as Animation;
};
