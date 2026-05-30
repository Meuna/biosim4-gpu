import { render, screen, fireEvent } from "@testing-library/svelte";
import CellPanel from "./CellPanel.svelte";
import type { AgentInfo } from "../workers/sim.worker";

const mockAgent: AgentInfo = {
    id: 42,
    alive: true,
    gx: 55,
    gy: 72,
    birthX: 10,
    birthY: 20,
    heading: 2,
    oscPeriod: 13,
    responsiveness: 0.75,
    losRange: 16,
    challengeBits: 0xdeadbeef,
    fingerprint: "0123456789abcdef",
};

const defaultProps = {
    agent: null as AgentInfo | null,
    isSelected: false,
    onClear: () => {},
    onNavigate: () => {},
    onShuffle: () => {},
    onSelectById: () => {},
    onExpandBrain: () => {},
};

describe("CellPanel", () => {
    it("shows empty state when agent is null", () => {
        render(CellPanel, { ...defaultProps });
        expect(screen.getByText("No selection")).toBeTruthy();
        expect(screen.getByText(/ctrl\+hover or click/i)).toBeTruthy();
    });

    it("shows shuffle button in empty state", () => {
        render(CellPanel, { ...defaultProps });
        expect(screen.getByText(/random agent/i)).toBeTruthy();
    });

    it("calls onShuffle from empty-state button", () => {
        let called = false;
        render(CellPanel, {
            ...defaultProps,
            onShuffle: () => {
                called = true;
            },
        });
        fireEvent.click(screen.getByText(/random agent/i));
        expect(called).toBe(true);
    });

    it("hides empty state when agent is provided", () => {
        render(CellPanel, { ...defaultProps, agent: mockAgent });
        expect(screen.queryByText("No selection")).toBeNull();
    });

    it("shows zero-padded agent id and hex id", () => {
        render(CellPanel, { ...defaultProps, agent: mockAgent });
        expect(screen.getByText(/#0042/)).toBeTruthy();
        expect(screen.getByText(/0x002A/)).toBeTruthy();
    });

    it("displays correct agent stats", () => {
        render(CellPanel, { ...defaultProps, agent: mockAgent });
        expect(screen.getByText("55, 72")).toBeTruthy(); // gx, gy
        expect(screen.getByText("10, 20")).toBeTruthy(); // birthX, birthY
        expect(screen.getByText("↑ N")).toBeTruthy(); // heading = 2
        expect(screen.getByText("13 steps")).toBeTruthy(); // oscPeriod
        expect(screen.getByText("0.75")).toBeTruthy(); // responsiveness
        expect(screen.getByText("16")).toBeTruthy(); // losRange
        expect(screen.getByText(/0x0123456789abcdef/i)).toBeTruthy(); // fingerprint
    });

    it("shows nav row when isSelected", () => {
        render(CellPanel, {
            ...defaultProps,
            agent: mockAgent,
            isSelected: true,
        });
        expect(screen.getByLabelText("Previous agent")).toBeTruthy();
        expect(screen.getByLabelText("Next agent")).toBeTruthy();
        expect(screen.getByLabelText("Random agent")).toBeTruthy();
    });

    it("shows hover-preview header when not isSelected", () => {
        render(CellPanel, {
            ...defaultProps,
            agent: mockAgent,
            isSelected: false,
        });
        expect(screen.getByText(/hover preview/i)).toBeTruthy();
        expect(screen.getByText(/click to pin/i)).toBeTruthy();
    });

    it("calls onNavigate(-1) when prev button clicked", () => {
        let dir: number | null = null;
        render(CellPanel, {
            ...defaultProps,
            agent: mockAgent,
            isSelected: true,
            onNavigate: (d) => {
                dir = d;
            },
        });
        fireEvent.click(screen.getByLabelText("Previous agent"));
        expect(dir).toBe(-1);
    });

    it("calls onNavigate(1) when next button clicked", () => {
        let dir: number | null = null;
        render(CellPanel, {
            ...defaultProps,
            agent: mockAgent,
            isSelected: true,
            onNavigate: (d) => {
                dir = d;
            },
        });
        fireEvent.click(screen.getByLabelText("Next agent"));
        expect(dir).toBe(1);
    });

    it("calls onShuffle from nav-row shuffle button", () => {
        let called = false;
        render(CellPanel, {
            ...defaultProps,
            agent: mockAgent,
            isSelected: true,
            onShuffle: () => {
                called = true;
            },
        });
        fireEvent.click(screen.getByLabelText("Random agent"));
        expect(called).toBe(true);
    });

    it("calls onClear when deselect is clicked", () => {
        let cleared = false;
        render(CellPanel, {
            ...defaultProps,
            agent: mockAgent,
            isSelected: true,
            onClear: () => {
                cleared = true;
            },
        });
        fireEvent.click(screen.getByText(/deselect/i));
        expect(cleared).toBe(true);
    });

    it("renders the brain signature with an expand affordance", () => {
        render(CellPanel, { ...defaultProps, agent: mockAgent });
        // agent id 42 → tiny fixture (6 × 4 × 3, 14 conn) in diagram mode.
        expect(
            screen.getByText("BRAIN · 6 IN → 4 HIDDEN → 3 OUT · 14 CONN"),
        ).toBeTruthy();
        expect(screen.getByLabelText(/expand brain/i)).toBeTruthy();
    });

    it("fires onExpandBrain when the signature expand is clicked", async () => {
        let expanded = false;
        render(CellPanel, {
            ...defaultProps,
            agent: mockAgent,
            onExpandBrain: () => {
                expanded = true;
            },
        });
        await fireEvent.click(screen.getByLabelText(/expand brain/i));
        expect(expanded).toBe(true);
    });

    it("shows skull icon for dead agent", () => {
        const deadAgent = { ...mockAgent, alive: false };
        render(CellPanel, { ...defaultProps, agent: deadAgent });
        expect(screen.getByLabelText("Agent deceased")).toBeTruthy();
    });

    it("applies dead class for dead agent", () => {
        const deadAgent = { ...mockAgent, alive: false };
        const { container } = render(CellPanel, {
            ...defaultProps,
            agent: deadAgent,
        });
        expect(container.querySelector(".cell-panel--dead")).toBeTruthy();
    });

    it("shows clickable id button when isSelected", () => {
        render(CellPanel, {
            ...defaultProps,
            agent: mockAgent,
            isSelected: true,
        });
        expect(screen.getByTitle("Click to jump to agent by ID")).toBeTruthy();
    });

    it("does not show id button when not isSelected", () => {
        render(CellPanel, {
            ...defaultProps,
            agent: mockAgent,
            isSelected: false,
        });
        expect(screen.queryByTitle("Click to jump to agent by ID")).toBeNull();
    });

    it("entering id and pressing Enter calls onSelectById", async () => {
        let received: number | null = null;
        render(CellPanel, {
            ...defaultProps,
            agent: mockAgent,
            isSelected: true,
            onSelectById: (id) => {
                received = id;
            },
        });
        fireEvent.click(screen.getByTitle("Click to jump to agent by ID"));
        const input = screen.getByLabelText("Jump to agent ID");
        fireEvent.input(input, { target: { value: "7" } });
        fireEvent.keyDown(input, { key: "Enter" });
        expect(received).toBe(7);
        expect(screen.queryByLabelText("Jump to agent ID")).toBeNull();
    });

    it("pressing Escape cancels without calling onSelectById", async () => {
        let called = false;
        render(CellPanel, {
            ...defaultProps,
            agent: mockAgent,
            isSelected: true,
            onSelectById: () => {
                called = true;
            },
        });
        fireEvent.click(screen.getByTitle("Click to jump to agent by ID"));
        const input = screen.getByLabelText("Jump to agent ID");
        fireEvent.keyDown(input, { key: "Escape" });
        expect(called).toBe(false);
        expect(screen.queryByLabelText("Jump to agent ID")).toBeNull();
    });
});
