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
