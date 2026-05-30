import { render, screen, fireEvent } from "@testing-library/svelte";
import BrainSignature from "./BrainSignature.svelte";
import { maxBrain, tinyBrain } from "./brain/fixture";

describe("BrainSignature", () => {
    it("renders the header line with correct counts", () => {
        render(BrainSignature, { model: tinyBrain, onExpand: () => {} });
        // tiny = 6 sense × 4 internal × 3 action, 14 conns
        expect(
            screen.getByText("BRAIN · 6 IN → 4 HIDDEN → 3 OUT · 14 CONN"),
        ).toBeTruthy();
    });

    it("renders diagram mode for a small brain", () => {
        const { container } = render(BrainSignature, {
            model: tinyBrain,
            onExpand: () => {},
        });
        expect(
            container.querySelector('svg[aria-label="Brain diagram"]'),
        ).toBeTruthy();
    });

    it("renders fingerprint mode for the max brain (above threshold)", () => {
        const { container } = render(BrainSignature, {
            model: maxBrain,
            onExpand: () => {},
        });
        expect(
            container.querySelector(
                'svg[aria-label="Brain signature fingerprint"]',
            ),
        ).toBeTruthy();
    });

    it("fires onExpand when the expand affordance is clicked", async () => {
        let expanded = false;
        render(BrainSignature, {
            model: tinyBrain,
            onExpand: () => {
                expanded = true;
            },
        });
        await fireEvent.click(screen.getByLabelText(/expand brain/i));
        expect(expanded).toBe(true);
    });
});
