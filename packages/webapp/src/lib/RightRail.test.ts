import { render, screen, fireEvent } from "@testing-library/svelte";
import { createRawSnippet } from "svelte";
import RightRail from "./RightRail.svelte";

describe("RightRail", () => {
    it("renders nothing when closed", () => {
        const { container } = render(RightRail, {
            open: false,
            activeTab: "sim",
            hasSelection: false,
            onTabChange: () => {},
        });
        expect(container.querySelector(".rail")).toBeNull();
    });

    it("renders the rail with tab buttons when open", () => {
        render(RightRail, {
            open: true,
            activeTab: "sim",
            hasSelection: false,
            onTabChange: () => {},
        });
        expect(screen.getByRole("tab", { name: /simulation/i })).toBeTruthy();
        expect(screen.getByRole("tab", { name: /cell/i })).toBeTruthy();
    });

    it("marks the active tab as selected", () => {
        render(RightRail, {
            open: true,
            activeTab: "cell",
            hasSelection: false,
            onTabChange: () => {},
        });
        const cellTab = screen.getByRole("tab", { name: /cell/i });
        expect(cellTab.getAttribute("aria-selected")).toBe("true");
        const simTab = screen.getByRole("tab", { name: /simulation/i });
        expect(simTab.getAttribute("aria-selected")).toBe("false");
    });

    it("calls onTabChange with correct tab when tab is clicked", () => {
        let switched = "";
        render(RightRail, {
            open: true,
            activeTab: "sim",
            hasSelection: false,
            onTabChange: (t: string) => {
                switched = t;
            },
        });
        fireEvent.click(screen.getByRole("tab", { name: /cell/i }));
        expect(switched).toBe("cell");
    });

    it("renders the simTabActions snippet in the tab bar when provided", () => {
        const simTabActions = createRawSnippet(() => ({
            render: () => `<button data-testid="sim-action">go</button>`,
        }));
        const { container } = render(RightRail, {
            open: true,
            activeTab: "sim",
            hasSelection: false,
            onTabChange: () => {},
            simTabActions,
        });
        expect(container.querySelector(".rail__tab-actions")).not.toBeNull();
        expect(screen.getByTestId("sim-action")).toBeTruthy();
    });

    it("omits the tab-actions container when no snippet is provided", () => {
        const { container } = render(RightRail, {
            open: true,
            activeTab: "sim",
            hasSelection: false,
            onTabChange: () => {},
        });
        expect(container.querySelector(".rail__tab-actions")).toBeNull();
    });
});
