import { render, screen } from "@testing-library/svelte";
import FatalErrorOverlay from "./FatalErrorOverlay.svelte";

describe("FatalErrorOverlay", () => {
    it("renders the message and a reload button", () => {
        render(FatalErrorOverlay, { message: "init failed" });
        expect(screen.getByRole("alertdialog")).toBeTruthy();
        expect(screen.getByText("init failed")).toBeTruthy();
        expect(screen.getByRole("button", { name: /reload/i })).toBeTruthy();
    });
});
