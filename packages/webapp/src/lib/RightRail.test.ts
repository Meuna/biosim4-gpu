import { render, screen, fireEvent } from "@testing-library/svelte";
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

    it("calls onClose when close button is clicked", () => {
        let closed = false;
        render(RightRail, {
            open: true,
            activeTab: "sim",
            hasSelection: false,
            onTabChange: () => {},
            onClose: () => {
                closed = true;
            },
        });
        fireEvent.click(screen.getByRole("button", { name: /close panel/i }));
        expect(closed).toBe(true);
    });

    it("renders no close button when onClose is not provided", () => {
        render(RightRail, {
            open: true,
            activeTab: "sim",
            hasSelection: false,
            onTabChange: () => {},
        });
        expect(
            screen.queryByRole("button", { name: /close panel/i }),
        ).toBeNull();
    });
});
