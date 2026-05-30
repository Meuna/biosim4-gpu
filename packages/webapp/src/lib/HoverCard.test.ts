import { describe, it, expect } from "vitest";
import { render } from "@testing-library/svelte";
import HoverCard from "./HoverCard.svelte";
import type { AgentInfo } from "../workers/sim.worker";

const mockAgent: AgentInfo = {
    id: 42,
    alive: true,
    gx: 10,
    gy: 20,
    birthX: 5,
    birthY: 5,
    heading: 2,
    oscPeriod: 5,
    responsiveness: 0.5,
    losRange: 3,
    challengeBits: 0,
    fingerprint: "abcdef1234567890",
};

describe("HoverCard", () => {
    it("renders nothing when agent is null", () => {
        const { container } = render(HoverCard, {
            props: { agent: null, x: 0, y: 0 },
        });
        expect(container.querySelector(".hover-card")).toBeNull();
    });

    it("renders the card when an agent is provided", () => {
        const { container } = render(HoverCard, {
            props: { agent: mockAgent, x: 100, y: 100 },
        });
        const card = container.querySelector(".hover-card");
        expect(card).not.toBeNull();
    });

    it("displays the agent id padded to 4 digits", () => {
        const { container } = render(HoverCard, {
            props: { agent: mockAgent, x: 0, y: 0 },
        });
        expect(container.textContent).toContain("#0042");
    });

    it("displays the agent coordinates", () => {
        const { container } = render(HoverCard, {
            props: { agent: mockAgent, x: 0, y: 0 },
        });
        expect(container.textContent).toContain("10, 20");
    });

    it("displays the agent direction", () => {
        const { container } = render(HoverCard, {
            props: { agent: mockAgent, x: 0, y: 0 },
        });
        expect(container.textContent).toContain("↑ N");
    });

    it("displays the gene fingerprint", () => {
        const { container } = render(HoverCard, {
            props: { agent: mockAgent, x: 0, y: 0 },
        });
        expect(container.textContent).toContain("0xabcdef1234567890");
    });
});
