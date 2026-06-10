import { describe, it, expect } from "vitest";
import type { AgentInfo, WorkerCmd } from "../workers/sim.worker";
import { AgentFocus } from "./agentFocus.svelte";

function agent(id: number): AgentInfo {
    return {
        id,
        alive: true,
        gx: id,
        gy: id,
        birthX: id,
        birthY: id,
        heading: 0,
        oscPeriod: 0,
        responsiveness: 1,
        losRange: 0,
        challengeBits: 0,
        fingerprint: id.toString(16).padStart(16, "0"),
    };
}

function create() {
    const sent: WorkerCmd[] = [];
    const f = new AgentFocus((cmd) => sent.push(cmd));
    return { f, sent };
}

function types(sent: WorkerCmd[]): string[] {
    return sent.map((c) => c.type);
}

describe("initial state", () => {
    it("is empty and sends nothing", () => {
        const { f, sent } = create();
        expect(f.selected).toBeNull();
        expect(f.hovered).toBeNull();
        expect(f.display).toBeNull();
        expect(f.displayId).toBeNull();
        expect(f.hasSelection).toBe(false);
        expect(f.isSelected).toBe(false);
        expect(sent).toEqual([]);
    });
});

describe("pick", () => {
    it("click sets the selection, clears last-hover, sends selectAgent", () => {
        const { f, sent } = create();
        f.pick(agent(7), "click");
        expect(f.selected?.id).toBe(7);
        expect(f.hovered).toBeNull();
        expect(f.display?.id).toBe(7);
        expect(f.displayId).toBe(7);
        expect(f.hasSelection).toBe(true);
        expect(f.isSelected).toBe(true);
        expect(sent).toEqual([{ type: "selectAgent", id: 7 }]);
    });

    it("hover sets hover + sticky last-hover, sends hoverAgent", () => {
        const { f, sent } = create();
        f.pick(agent(3), "hover");
        expect(f.hovered?.id).toBe(3);
        expect(f.display?.id).toBe(3);
        expect(f.isSelected).toBe(false);
        expect(sent).toEqual([{ type: "hoverAgent", id: 3 }]);
    });

    it("hover overrides an existing selection in display", () => {
        const { f } = create();
        f.pick(agent(1), "click");
        f.pick(agent(2), "hover");
        expect(f.selected?.id).toBe(1);
        expect(f.display?.id).toBe(2);
        expect(f.isSelected).toBe(false);
    });
});

describe("miss", () => {
    it("click clears the selection and sends selectAgent:null", () => {
        const { f, sent } = create();
        f.pick(agent(7), "click");
        f.miss("click");
        expect(f.selected).toBeNull();
        expect(sent.at(-1)).toEqual({ type: "selectAgent", id: null });
    });

    it("hover clears the live hover and sends hoverAgent:null", () => {
        const { f, sent } = create();
        f.pick(agent(3), "hover");
        f.miss("hover");
        expect(f.hovered).toBeNull();
        expect(sent.at(-1)).toEqual({ type: "hoverAgent", id: null });
    });
});

describe("update", () => {
    it("replaces the selected agent info when a selection exists", () => {
        const { f } = create();
        f.pick(agent(7), "click");
        const moved = { ...agent(7), gx: 99 };
        f.update(moved);
        expect(f.selected?.gx).toBe(99);
    });

    it("is a no-op (race guard) when there is no selection", () => {
        const { f, sent } = create();
        f.update(agent(7));
        expect(f.selected).toBeNull();
        expect(sent).toEqual([]);
    });
});

describe("promoteHoverToSelection", () => {
    it("promotes the sticky last-hover after the live hover ended", () => {
        const { f, sent } = create();
        f.pick(agent(5), "hover");
        f.endHover();
        const acted = f.promoteHoverToSelection();
        expect(acted).toBe(true);
        expect(f.selected?.id).toBe(5);
        expect(f.isSelected).toBe(true);
        expect(sent.at(-1)).toEqual({ type: "selectAgent", id: 5 });
    });

    it("returns false and sends nothing when there is no last-hover", () => {
        const { f, sent } = create();
        expect(f.promoteHoverToSelection()).toBe(false);
        expect(sent).toEqual([]);
    });

    it("returns false while a live hover is still in play", () => {
        const { f } = create();
        f.pick(agent(5), "hover");
        expect(f.promoteHoverToSelection()).toBe(false);
    });
});

describe("endHover", () => {
    it("clears the live hover but keeps the sticky last-hover", () => {
        const { f, sent } = create();
        f.pick(agent(5), "hover");
        f.endHover();
        expect(f.hovered).toBeNull();
        expect(f.display?.id).toBe(5); // sticky last-hover still displayed
        expect(sent.at(-1)).toEqual({ type: "hoverAgent", id: null });
    });

    it("sends nothing when there is no live hover", () => {
        const { f, sent } = create();
        f.endHover();
        expect(sent).toEqual([]);
    });
});

describe("clearHover", () => {
    it("clears both the live and sticky hover and sends hoverAgent:null", () => {
        const { f, sent } = create();
        f.pick(agent(5), "hover");
        f.clearHover();
        expect(f.hovered).toBeNull();
        expect(f.display).toBeNull();
        expect(sent.at(-1)).toEqual({ type: "hoverAgent", id: null });
    });
});

describe("clearSelection", () => {
    it("clears the whole triad and sends both null commands", () => {
        const { f, sent } = create();
        f.pick(agent(7), "click");
        sent.length = 0;
        f.clearSelection();
        expect(f.selected).toBeNull();
        expect(f.hovered).toBeNull();
        expect(f.display).toBeNull();
        expect(sent).toEqual([
            { type: "selectAgent", id: null },
            { type: "hoverAgent", id: null },
        ]);
    });
});

describe("command pass-throughs", () => {
    it("navigate sends navigateAgent with the selected id as fromId", () => {
        const { f, sent } = create();
        f.pick(agent(7), "click");
        sent.length = 0;
        f.navigate(1);
        expect(sent).toEqual([
            { type: "navigateAgent", fromId: 7, direction: 1 },
        ]);
    });

    it("navigate is a no-op without a selection", () => {
        const { f, sent } = create();
        f.navigate(-1);
        expect(sent).toEqual([]);
    });

    it("shuffle sends randomAgent", () => {
        const { f, sent } = create();
        f.shuffle();
        expect(types(sent)).toEqual(["randomAgent"]);
    });

    it("selectById sends selectAgentById", () => {
        const { f, sent } = create();
        f.selectById(42);
        expect(sent).toEqual([{ type: "selectAgentById", id: 42 }]);
    });
});

describe("display priority", () => {
    it("is hovered ?? lastHovered ?? selected", () => {
        const { f } = create();
        f.pick(agent(1), "click");
        expect(f.display?.id).toBe(1); // selected only

        f.pick(agent(2), "hover");
        expect(f.display?.id).toBe(2); // live hover wins

        f.endHover();
        expect(f.display?.id).toBe(2); // sticky last-hover over selection

        f.clearHover();
        expect(f.display?.id).toBe(1); // back to selection
    });
});
