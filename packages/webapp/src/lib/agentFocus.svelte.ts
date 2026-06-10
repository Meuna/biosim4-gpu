import type { AgentInfo, WorkerCmd } from "../workers/sim.worker";

export type SendFn = (cmd: WorkerCmd) => void;

// The agent-inspection focus controller. Owns the focus triad
// { selected, hovered, lastHovered }, the derived `display` agent, and every
// agent-related worker command. UX gestures and worker replies call its intent
// methods; the rest of the app only reads its getters. No `$effect` lives
// inside the class — methods mutate state imperatively to avoid self-trigger
// loops.
//
// `display` priority is hover-then-sticky-then-selection: a live hover wins,
// then the last hovered agent (kept so a Ctrl+click can promote it), then the
// persistent selection.
export class AgentFocus {
    #selected = $state<AgentInfo | null>(null);
    #hovered = $state<AgentInfo | null>(null);
    #lastHovered = $state<AgentInfo | null>(null);

    readonly #send: SendFn;

    #display = $derived(this.#hovered ?? this.#lastHovered ?? this.#selected);

    // True when the persistent selection is the active display — i.e. no hover
    // (live or sticky) is overriding it. Drives the Cell panel's "selected"
    // affordance.
    #isSelected = $derived(
        this.#selected !== null &&
            this.#hovered === null &&
            this.#lastHovered === null,
    );

    constructor(send: SendFn) {
        this.#send = send;
    }

    get selected(): AgentInfo | null {
        return this.#selected;
    }

    get hovered(): AgentInfo | null {
        return this.#hovered;
    }

    get display(): AgentInfo | null {
        return this.#display;
    }

    get displayId(): number | null {
        return this.#display?.id ?? null;
    }

    get hasSelection(): boolean {
        return this.#display !== null;
    }

    get isSelected(): boolean {
        return this.#isSelected;
    }

    // ── Worker-reply intents ──────────────────────────────────────────────────

    // Reply to a pickAgentAtCell / navigate / random / selectById gesture.
    pick(info: AgentInfo, reason: "click" | "hover"): void {
        if (reason === "click") {
            this.#selected = info;
            this.#lastHovered = null;
            this.#send({ type: "selectAgent", id: info.id });
        } else {
            this.#hovered = info;
            this.#lastHovered = info;
            this.#send({ type: "hoverAgent", id: info.id });
        }
    }

    // Reply when a pick landed on an empty cell.
    miss(reason: "click" | "hover"): void {
        if (reason === "click") {
            this.#selected = null;
            this.#send({ type: "selectAgent", id: null });
        } else {
            this.#hovered = null;
            this.#send({ type: "hoverAgent", id: null });
        }
    }

    // Live per-step feed for the selected agent. Guarded against resurrecting a
    // just-cleared selection: a same-tick agentUpdated may land after we sent
    // selectAgent:null, so only apply it while a selection still exists.
    update(info: AgentInfo): void {
        if (this.#selected !== null) this.#selected = info;
    }

    // ── User gestures ─────────────────────────────────────────────────────────

    // Ctrl+click promotes the sticky last-hover to a selection. Returns whether
    // it acted, so the click handler can skip the cell-pick path.
    promoteHoverToSelection(): boolean {
        if (this.#lastHovered === null || this.#hovered !== null) return false;
        this.#selected = this.#lastHovered;
        this.#lastHovered = null;
        this.#send({ type: "selectAgent", id: this.#selected.id });
        return true;
    }

    // Mouse left the grid / Ctrl released mid-move: drop the live hover only,
    // keeping the sticky last-hover for a possible Ctrl+click promotion.
    endHover(): void {
        if (this.#hovered !== null) {
            this.#hovered = null;
            this.#send({ type: "hoverAgent", id: null });
        }
    }

    // Ctrl key released: drop the live hover and the sticky last-hover.
    clearHover(): void {
        this.#hovered = null;
        this.#lastHovered = null;
        this.#send({ type: "hoverAgent", id: null });
    }

    clearSelection(): void {
        this.#selected = null;
        this.#hovered = null;
        this.#lastHovered = null;
        this.#send({ type: "selectAgent", id: null });
        this.#send({ type: "hoverAgent", id: null });
    }

    // ── Command pass-throughs (reply returns via pick()) ──────────────────────

    navigate(direction: -1 | 1): void {
        if (this.#selected) {
            this.#send({
                type: "navigateAgent",
                fromId: this.#selected.id,
                direction,
            });
        }
    }

    shuffle(): void {
        this.#send({ type: "randomAgent" });
    }

    selectById(id: number): void {
        this.#send({ type: "selectAgentById", id });
    }
}
