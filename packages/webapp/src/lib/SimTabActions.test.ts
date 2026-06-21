import { render, screen, fireEvent } from "@testing-library/svelte";
import { vi } from "vitest";
import SimTabActions from "./SimTabActions.svelte";

const defaultProps = {
    dirty: false,
    disabled: false,
    onRevert: () => {},
    onNextGen: () => {},
    onRewind: () => {},
};

describe("SimTabActions", () => {
    it("renders revert, next-gen and rewind buttons", () => {
        render(SimTabActions, { ...defaultProps });
        expect(
            screen.getByRole("button", { name: "Revert all changes" }),
        ).toBeTruthy();
        expect(
            screen.getByRole("button", { name: /advance one generation/i }),
        ).toBeTruthy();
        expect(screen.getByRole("button", { name: /rewind/i })).toBeTruthy();
    });

    it("disables revert when not dirty and enables it when dirty", () => {
        const { rerender } = render(SimTabActions, { ...defaultProps });
        const revert = screen.getByRole("button", {
            name: "Revert all changes",
        }) as HTMLButtonElement;
        expect(revert.disabled).toBe(true);

        rerender({ ...defaultProps, dirty: true });
        expect(revert.disabled).toBe(false);
    });

    it("calls onRevert when the revert button is clicked", async () => {
        const onRevert = vi.fn();
        render(SimTabActions, { ...defaultProps, dirty: true, onRevert });
        await fireEvent.click(
            screen.getByRole("button", { name: "Revert all changes" }),
        );
        expect(onRevert).toHaveBeenCalledOnce();
    });

    it("maps ctrl-click to autoPlay for next-gen and rewind", async () => {
        const onNextGen = vi.fn();
        const onRewind = vi.fn();
        render(SimTabActions, { ...defaultProps, onNextGen, onRewind });

        await fireEvent.click(
            screen.getByRole("button", { name: /advance one generation/i }),
        );
        expect(onNextGen).toHaveBeenLastCalledWith(false);
        await fireEvent.click(
            screen.getByRole("button", { name: /advance one generation/i }),
            { ctrlKey: true },
        );
        expect(onNextGen).toHaveBeenLastCalledWith(true);

        await fireEvent.click(screen.getByRole("button", { name: /rewind/i }), {
            ctrlKey: true,
        });
        expect(onRewind).toHaveBeenLastCalledWith(true);
    });

    it("disables next-gen and rewind when disabled is true", () => {
        render(SimTabActions, { ...defaultProps, dirty: true, disabled: true });
        const nextGen = screen.getByRole("button", {
            name: /advance one generation/i,
        }) as HTMLButtonElement;
        const rewind = screen.getByRole("button", {
            name: /rewind/i,
        }) as HTMLButtonElement;
        expect(nextGen.disabled).toBe(true);
        expect(rewind.disabled).toBe(true);
    });
});
