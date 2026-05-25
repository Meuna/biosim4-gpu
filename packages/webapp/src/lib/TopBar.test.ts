import { render, screen } from "@testing-library/svelte";
import TopBar from "./TopBar.svelte";

describe("TopBar", () => {
    it("renders the brand name and subtitle", () => {
        render(TopBar, { running: false, gen: 0, step: 0, stepsPerGen: 300 });
        expect(screen.getByText("biosim4-gpu")).toBeTruthy();
        expect(screen.getByText("visualizer · v0.1")).toBeTruthy();
    });

    it("shows idle chip when not running", () => {
        render(TopBar, { running: false, gen: 0, step: 0, stepsPerGen: 300 });
        expect(screen.getByText("idle")).toBeTruthy();
    });

    it("shows running chip with gen and step when running", () => {
        render(TopBar, { running: true, gen: 5, step: 42, stepsPerGen: 300 });
        const chipText = screen.getByText("running · gen 5 · step 42/300");
        expect(chipText).toBeTruthy();
        expect(screen.queryByText("idle")).toBeNull();
    });

    it("renders the GitHub link", () => {
        render(TopBar, { running: false, gen: 0, step: 0, stepsPerGen: 300 });
        const link = screen.getByRole("link", { name: /github/i });
        expect(link).toBeTruthy();
        expect((link as HTMLAnchorElement).href).toContain("github.com");
    });
});
