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

    it("shows width slider for hbar", () => {
        render(BarrierControl, {
            props: { value: [defaultHbar], onchange: vi.fn() },
        });
        expect(screen.queryByLabelText("Width")).toBeTruthy();
    });

    it("shows width slider for vbar", () => {
        const vbar: BarrierSpec = { ...defaultHbar, kind: "vbar" };
        render(BarrierControl, {
            props: { value: [vbar], onchange: vi.fn() },
        });
        expect(screen.queryByLabelText("Width")).toBeTruthy();
    });

    it("hides width slider for square", () => {
        const square: BarrierSpec = {
            ...defaultHbar,
            kind: "square",
            width: null,
        };
        render(BarrierControl, {
            props: { value: [square], onchange: vi.fn() },
        });
        expect(screen.queryByLabelText("Width")).toBeNull();
    });

    it("hides width slider for circle", () => {
        const circle: BarrierSpec = {
            ...defaultHbar,
            kind: "circle",
            width: null,
        };
        render(BarrierControl, {
            props: { value: [circle], onchange: vi.fn() },
        });
        expect(screen.queryByLabelText("Width")).toBeNull();
    });

    it("shuffle button calls onchange with randomised values", async () => {
        const onchange = vi.fn<[BarrierSpec[]], void>();
        render(BarrierControl, {
            props: { value: [defaultHbar], onchange },
        });
        await fireEvent.click(screen.getByLabelText("Shuffle barrier 1"));
        expect(onchange).toHaveBeenCalledOnce();
        const barriers = onchange.mock.calls[0][0];
        expect(typeof barriers[0].x).toBe("number");
        expect(typeof barriers[0].y).toBe("number");
        expect(typeof barriers[0].length).toBe("number");
        expect(typeof barriers[0].width).toBe("number");
    });

    it("shuffle button sets width to null for square", async () => {
        const onchange = vi.fn<[BarrierSpec[]], void>();
        const square: BarrierSpec = {
            ...defaultHbar,
            kind: "square",
            width: null,
        };
        render(BarrierControl, {
            props: { value: [square], onchange },
        });
        await fireEvent.click(screen.getByLabelText("Shuffle barrier 1"));
        const barriers = onchange.mock.calls[0][0];
        expect(barriers[0].width).toBeNull();
    });

    it("collapse toggle hides controls and shows summary", async () => {
        render(BarrierControl, {
            props: { value: [defaultHbar], onchange: vi.fn() },
        });
        expect(screen.queryByLabelText("Barrier 1 kind")).toBeTruthy();
        await fireEvent.click(screen.getByLabelText("Toggle barrier 1"));
        expect(screen.queryByLabelText("Barrier 1 kind")).toBeNull();
        expect(screen.getByText(/hbar\(/)).toBeTruthy();
    });

    it("collapse toggle re-expands on second click", async () => {
        render(BarrierControl, {
            props: { value: [defaultHbar], onchange: vi.fn() },
        });
        const toggle = screen.getByLabelText("Toggle barrier 1");
        await fireEvent.click(toggle);
        await fireEvent.click(toggle);
        expect(screen.queryByLabelText("Barrier 1 kind")).toBeTruthy();
    });

    it("disables the add button and per-barrier controls when disabled", () => {
        render(BarrierControl, {
            props: { value: [defaultHbar], disabled: true, onchange: vi.fn() },
        });
        expect(
            (screen.getByLabelText("Add barrier") as HTMLButtonElement)
                .disabled,
        ).toBe(true);
        expect(
            (screen.getByLabelText("Barrier 1 kind") as HTMLSelectElement)
                .disabled,
        ).toBe(true);
        expect(
            (screen.getByLabelText("Remove barrier 1") as HTMLButtonElement)
                .disabled,
        ).toBe(true);
    });

    it("shows warning when near_barrier challenge has no barriers", () => {
        render(BarrierControl, {
            props: {
                value: [],
                challengeKind: "near_barrier",
                onchange: vi.fn(),
            },
        });
        expect(screen.getByText(/add a barrier here/i)).toBeTruthy();
    });

    it("hides warning when near_barrier challenge has barriers", () => {
        render(BarrierControl, {
            props: {
                value: [defaultHbar],
                challengeKind: "near_barrier",
                onchange: vi.fn(),
            },
        });
        expect(screen.queryByText(/add a barrier here/i)).toBeNull();
    });

    it("hides warning for other challenge kinds", () => {
        render(BarrierControl, {
            props: {
                value: [],
                challengeKind: "disc",
                onchange: vi.fn(),
            },
        });
        expect(screen.queryByText(/add a barrier here/i)).toBeNull();
    });
});
