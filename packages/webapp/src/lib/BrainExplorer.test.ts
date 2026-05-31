import { render, screen, fireEvent } from "@testing-library/svelte";
import BrainExplorer from "./BrainExplorer.svelte";
import { GENE_IO, GENE_NEURON, type BrainConn } from "./brain";

// sensor 0 (X, text label) → neuron 0 → action 8 (MOVE_RIGHT, a glyph).
const conns: BrainConn[] = [
    {
        srcType: GENE_IO,
        srcNum: 0,
        sinkType: GENE_NEURON,
        sinkNum: 0,
        weight: 1,
    },
    {
        srcType: GENE_NEURON,
        srcNum: 0,
        sinkType: GENE_IO,
        sinkNum: 8,
        weight: -1,
    },
];

describe("BrainExplorer", () => {
    it("renders the sense node's text label", () => {
        render(BrainExplorer, { conns, neuronCount: 1, variant: "preview" });
        expect(screen.getByText("X")).toBeTruthy(); // sensor 0 (text label)
    });

    it("shows the full name of a selected sense, nothing for a neuron", async () => {
        render(BrainExplorer, { conns, neuronCount: 1, variant: "preview" });
        expect(screen.queryByText("Location X")).toBeNull();
        await fireEvent.click(screen.getByLabelText("sense X"));
        expect(screen.getByText("Location X")).toBeTruthy();
        // Selecting a neuron shows no name text.
        await fireEvent.click(screen.getByLabelText("neuron 0"));
        expect(screen.queryByText("Location X")).toBeNull();
        expect(screen.queryByText(/internal neuron/i)).toBeNull();
    });

    it("renders one node per distinct sense, action and neuron", () => {
        const { container } = render(BrainExplorer, {
            conns,
            neuronCount: 1,
            variant: "preview",
        });
        // s0 + a8 + n0 = 3 nodes.
        expect(container.querySelectorAll(".brain__node").length).toBe(3);
        // Two connections, drawn as <line> (preview has no legend lines).
        expect(container.querySelectorAll(".brain__links line").length).toBe(2);
    });

    it("shows the synthesis line in the full variant", () => {
        render(BrainExplorer, { conns, neuronCount: 1, variant: "full" });
        expect(
            screen.getByText(
                /1 senses → 1 internal → 1 actions · 2 connections/,
            ),
        ).toBeTruthy();
    });

    it("dims non-incident connections when a node is focused", async () => {
        const { container } = render(BrainExplorer, {
            conns,
            neuronCount: 1,
            variant: "preview",
        });
        const lines = () =>
            [...container.querySelectorAll(".brain__links line")].map((l) =>
                l.getAttribute("opacity"),
            );
        // Default: every edge at half opacity.
        expect(lines()).toEqual(["0.5", "0.5"]);
        // Focus the sense node — the neuron→action edge is no longer incident
        // (0.05), while the sense→neuron edge is highlighted to full opacity.
        await fireEvent.click(screen.getByLabelText("sense X"));
        expect(lines().toSorted()).toEqual(["0.05", "1"]);
    });

    it("shows the expand button only in preview with onExpand", async () => {
        let expanded = false;
        render(BrainExplorer, {
            conns,
            neuronCount: 1,
            variant: "preview",
            onExpand: () => (expanded = true),
        });
        await fireEvent.click(screen.getByLabelText("Expand brain explorer"));
        expect(expanded).toBe(true);
    });

    it("renders without throwing for an empty brain", () => {
        const { container } = render(BrainExplorer, {
            conns: [],
            neuronCount: 0,
            variant: "preview",
        });
        expect(container.querySelectorAll(".brain__node").length).toBe(0);
    });
});
