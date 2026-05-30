import { render, screen, fireEvent } from "@testing-library/svelte";
import BrainExplorer from "./BrainExplorer.svelte";
import { maxBrain, mediumBrain, tinyBrain } from "./brain/fixture";

// jsdom has no 2d canvas context, so draw() short-circuits; these tests verify
// structure, controls and dismissal rather than pixels. jsdom also lacks
// ResizeObserver (used by bind:clientWidth), so stub it.
beforeAll(() => {
    globalThis.ResizeObserver = class {
        observe() {}
        unobserve() {}
        disconnect() {}
    } as unknown as typeof ResizeObserver;
});

describe("BrainExplorer", () => {
    it("renders every fixture without throwing", () => {
        for (const model of [tinyBrain, mediumBrain, maxBrain]) {
            const { unmount } = render(BrainExplorer, {
                model,
                onClose: () => {},
            });
            unmount();
        }
    });

    it("shows the count header", () => {
        render(BrainExplorer, { model: tinyBrain, onClose: () => {} });
        expect(
            screen.getByText(/6 IN · 4 HIDDEN · 3 OUT · 14 CONN/),
        ).toBeTruthy();
    });

    it("closes on the close button", async () => {
        let closed = false;
        render(BrainExplorer, {
            model: tinyBrain,
            onClose: () => {
                closed = true;
            },
        });
        await fireEvent.click(screen.getByLabelText("Close explorer"));
        expect(closed).toBe(true);
    });

    it("closes on Escape", async () => {
        let closed = false;
        render(BrainExplorer, {
            model: tinyBrain,
            onClose: () => {
                closed = true;
            },
        });
        await fireEvent.keyDown(window, { key: "Escape" });
        expect(closed).toBe(true);
    });

    it("activates the anti-hairball default for a dense brain", () => {
        render(BrainExplorer, { model: maxBrain, onClose: () => {} });
        // Dense hint shown and threshold pre-set above zero.
        expect(screen.getByText(/dense brain/i)).toBeTruthy();
        expect(screen.queryByText(/weight threshold · 0\.00/)).toBeNull();
    });

    it("does not anti-hairball a small brain", () => {
        render(BrainExplorer, { model: tinyBrain, onClose: () => {} });
        expect(screen.queryByText(/dense brain/i)).toBeNull();
        expect(screen.getByText(/weight threshold · 0\.00/)).toBeTruthy();
    });

    it("exposes type and recurrent filters and neuron search", () => {
        render(BrainExplorer, { model: mediumBrain, onClose: () => {} });
        expect(screen.getByText("sense")).toBeTruthy();
        expect(screen.getByText("internal")).toBeTruthy();
        expect(screen.getByText("action")).toBeTruthy();
        expect(screen.getByText("recurrent edges")).toBeTruthy();
        expect(screen.getByLabelText("jump to neuron")).toBeTruthy();
    });
});
