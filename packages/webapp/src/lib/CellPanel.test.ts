import { render, screen, fireEvent } from "@testing-library/svelte";
import CellPanel from "./CellPanel.svelte";

const mockAgent = {
    id: 42,
    gen: 3,
    age: 127,
    gx: 55,
    gy: 72,
    heading: 2,
};

describe("CellPanel", () => {
    it("shows empty state when agent is null", () => {
        render(CellPanel, { agent: null, onClear: () => {} });
        expect(screen.getByText("No selection")).toBeTruthy();
        expect(screen.getByText(/click any cell/i)).toBeTruthy();
    });

    it("hides empty state when agent is provided", () => {
        render(CellPanel, { agent: mockAgent, onClear: () => {} });
        expect(screen.queryByText("No selection")).toBeNull();
    });

    it("shows zero-padded agent id and hex id", () => {
        render(CellPanel, { agent: mockAgent, onClear: () => {} });
        expect(screen.getByText(/Cell/)).toBeTruthy();
        expect(screen.getByText(/#0042/)).toBeTruthy();
        expect(screen.getByText(/0x002A/)).toBeTruthy();
    });

    it("displays correct agent stats", () => {
        render(CellPanel, { agent: mockAgent, onClear: () => {} });
        expect(screen.getByText("3")).toBeTruthy(); // gen
        expect(screen.getByText("55, 72")).toBeTruthy(); // x, y
        expect(screen.getByText("↑ N")).toBeTruthy(); // heading = 2
    });

    it("calls onClear when deselect is clicked", () => {
        let cleared = false;
        render(CellPanel, {
            agent: mockAgent,
            onClear: () => {
                cleared = true;
            },
        });
        fireEvent.click(screen.getByText(/deselect/i));
        expect(cleared).toBe(true);
    });

    it("renders brain and trace placeholders", () => {
        render(CellPanel, { agent: mockAgent, onClear: () => {} });
        expect(screen.getByLabelText("Brain graph placeholder")).toBeTruthy();
        expect(
            screen.getByLabelText("Movement trace placeholder"),
        ).toBeTruthy();
    });
});
