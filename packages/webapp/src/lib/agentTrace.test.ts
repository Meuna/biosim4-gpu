import { appendTracePoint, type TracePoint } from "./agentTrace";

describe("appendTracePoint", () => {
    it("appends the first point", () => {
        const trace: TracePoint[] = [];
        appendTracePoint(trace, 3, 4);
        expect(trace).toEqual([{ gx: 3, gy: 4 }]);
    });

    it("keeps distinct cells in order", () => {
        const trace: TracePoint[] = [];
        appendTracePoint(trace, 0, 0);
        appendTracePoint(trace, 1, 0);
        appendTracePoint(trace, 1, 1);
        expect(trace).toEqual([
            { gx: 0, gy: 0 },
            { gx: 1, gy: 0 },
            { gx: 1, gy: 1 },
        ]);
    });

    it("skips a consecutive duplicate cell", () => {
        const trace: TracePoint[] = [];
        appendTracePoint(trace, 2, 2);
        appendTracePoint(trace, 2, 2);
        expect(trace).toEqual([{ gx: 2, gy: 2 }]);
    });

    it("re-appends a cell revisited after moving away", () => {
        const trace: TracePoint[] = [];
        appendTracePoint(trace, 5, 5);
        appendTracePoint(trace, 6, 5);
        appendTracePoint(trace, 5, 5);
        expect(trace).toEqual([
            { gx: 5, gy: 5 },
            { gx: 6, gy: 5 },
            { gx: 5, gy: 5 },
        ]);
    });

    it("drops the oldest point past the cap", () => {
        const trace: TracePoint[] = [];
        appendTracePoint(trace, 0, 0, 2);
        appendTracePoint(trace, 1, 1, 2);
        appendTracePoint(trace, 2, 2, 2);
        expect(trace).toEqual([
            { gx: 1, gy: 1 },
            { gx: 2, gy: 2 },
        ]);
    });

    it("returns the same array instance", () => {
        const trace: TracePoint[] = [];
        expect(appendTracePoint(trace, 1, 1)).toBe(trace);
    });
});
