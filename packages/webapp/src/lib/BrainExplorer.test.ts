import { render, screen, fireEvent } from "@testing-library/svelte";
import BrainExplorer from "./BrainExplorer.svelte";
import { GENE_IO, GENE_NEURON, type BrainConn } from "./brain";

// sensor 0 (X) → neuron 0 → action 8 (MOVE_RIGHT, "↱").
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
    it("renders sense and action labels", () => {
        render(BrainExplorer, { conns, neuronCount: 1, variant: "preview" });
        expect(screen.getByText("X")).toBeTruthy(); // sensor 0
        expect(screen.getByText("↱")).toBeTruthy(); // action 8
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
        expect(container.querySelectorAll(".brain__edge--dim").length).toBe(0);
        // Focus the sense node — the neuron→action edge is no longer incident.
        await fireEvent.click(screen.getByLabelText("sense X"));
        expect(container.querySelectorAll(".brain__edge--dim").length).toBe(1);
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
