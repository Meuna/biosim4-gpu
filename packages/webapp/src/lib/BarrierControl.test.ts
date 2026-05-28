import { render, screen, fireEvent } from "@testing-library/svelte";
import { vi } from "vitest";
import BarrierControl from "./BarrierControl.svelte";
import type { BarrierSpec } from "../workers/sim.worker";

const defaultHbar: BarrierSpec = {
    kind: "hbar",
    x: 0.5,
    y: 0.5,
    length: 0.25,
    width: 0.02,
};

describe("BarrierControl", () => {
    it("renders the add barrier button", () => {
        render(BarrierControl, { props: { value: [], onchange: vi.fn() } });
        expect(screen.getByLabelText("Add barrier")).toBeTruthy();
    });

    it("clicking add barrier calls onchange with one barrier", async () => {
        const onchange = vi.fn<[BarrierSpec[]], void>();
        render(BarrierControl, { props: { value: [], onchange } });
        await fireEvent.click(screen.getByLabelText("Add barrier"));
        expect(onchange).toHaveBeenCalledOnce();
        const barriers = onchange.mock.calls[0][0];
        expect(barriers).toHaveLength(1);
        expect(barriers[0].kind).toBe("hbar");
    });

    it("remove barrier button removes the barrier", async () => {
        const onchange = vi.fn<[BarrierSpec[]], void>();
        render(BarrierControl, {
            props: { value: [defaultHbar], onchange },
        });
        await fireEvent.click(screen.getByLabelText("Remove barrier 1"));
        expect(onchange).toHaveBeenCalledOnce();
        expect(onchange.mock.calls[0][0]).toHaveLength(0);
    });

    it("shows kind dropdown for each barrier", () => {
        render(BarrierControl, {
            props: { value: [defaultHbar], onchange: vi.fn() },
        });
        const select = screen.getByLabelText(
            "Barrier 1 kind",
        ) as HTMLSelectElement;
        expect(select.value).toBe("hbar");
    });

    it("changing kind calls onchange with updated kind", async () => {
        const onchange = vi.fn<[BarrierSpec[]], void>();
        render(BarrierControl, {
            props: { value: [defaultHbar], onchange },
        });
        await fireEvent.change(screen.getByLabelText("Barrier 1 kind"), {
            target: { value: "circle" },
        });
        expect(onchange).toHaveBeenCalledOnce();
        const barriers = onchange.mock.calls[0][0];
        expect(barriers[0].kind).toBe("circle");
    });

    it("shows width field for hbar", () => {
        render(BarrierControl, {
            props: { value: [defaultHbar], onchange: vi.fn() },
        });
        expect(screen.queryByLabelText("Barrier 1 width random")).toBeTruthy();
    });

    it("shows width field for vbar", () => {
        const vbar: BarrierSpec = { ...defaultHbar, kind: "vbar" };
        render(BarrierControl, {
            props: { value: [vbar], onchange: vi.fn() },
        });
        expect(screen.queryByLabelText("Barrier 1 width random")).toBeTruthy();
    });

    it("hides width field for square", () => {
        const square: BarrierSpec = {
            ...defaultHbar,
            kind: "square",
            width: null,
        };
        render(BarrierControl, {
            props: { value: [square], onchange: vi.fn() },
        });
        expect(screen.queryByLabelText("Barrier 1 width random")).toBeNull();
    });

    it("hides width field for circle", () => {
        const circle: BarrierSpec = {
            ...defaultHbar,
            kind: "circle",
            width: null,
        };
        render(BarrierControl, {
            props: { value: [circle], onchange: vi.fn() },
        });
        expect(screen.queryByLabelText("Barrier 1 width random")).toBeNull();
    });

    it("checking X random sets x to null in emitted spec", async () => {
        const onchange = vi.fn<[BarrierSpec[]], void>();
        render(BarrierControl, {
            props: { value: [defaultHbar], onchange },
        });
        await fireEvent.click(screen.getByLabelText("Barrier 1 X random"));
        const barriers = onchange.mock.calls[0][0];
        expect(barriers[0].x).toBeNull();
    });

    it("unchecking X random sets x to 0.5 in emitted spec", async () => {
        const onchange = vi.fn<[BarrierSpec[]], void>();
        const randomX: BarrierSpec = { ...defaultHbar, x: null };
        render(BarrierControl, {
            props: { value: [randomX], onchange },
        });
        await fireEvent.click(screen.getByLabelText("Barrier 1 X random"));
        const barriers = onchange.mock.calls[0][0];
        expect(barriers[0].x).toBe(0.5);
    });

    it("shows warning when near_barrier challenge has no barriers", () => {
        render(BarrierControl, {
            props: {
                value: [],
                challengeKind: "near_barrier",
                onchange: vi.fn(),
            },
        });
        expect(
            screen.getByText(
                /near-barrier challenges require at least one barrier/i,
            ),
        ).toBeTruthy();
    });

    it("hides warning when near_barrier challenge has barriers", () => {
        render(BarrierControl, {
            props: {
                value: [defaultHbar],
                challengeKind: "near_barrier",
                onchange: vi.fn(),
            },
        });
        expect(
            screen.queryByText(
                /near-barrier challenges require at least one barrier/i,
            ),
        ).toBeNull();
    });

    it("hides warning for other challenge kinds", () => {
        render(BarrierControl, {
            props: {
                value: [],
                challengeKind: "disc",
                onchange: vi.fn(),
            },
        });
        expect(
            screen.queryByText(
                /near-barrier challenges require at least one barrier/i,
            ),
        ).toBeNull();
    });
});
