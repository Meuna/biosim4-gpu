import { render, screen, fireEvent } from "@testing-library/svelte";
import ErrorBanner from "./ErrorBanner.svelte";

describe("ErrorBanner", () => {
    it("renders the message", () => {
        render(ErrorBanner, { message: "Boom", onClose: () => {} });
        expect(screen.getByRole("alert")).toBeTruthy();
        expect(screen.getByText("Boom")).toBeTruthy();
    });

    it("calls onClose when the dismiss button is clicked", async () => {
        let called = false;
        render(ErrorBanner, {
            message: "Boom",
            onClose: () => {
                called = true;
            },
        });
        await fireEvent.click(
            screen.getByRole("button", { name: /dismiss error/i }),
        );
        expect(called).toBe(true);
    });
});
