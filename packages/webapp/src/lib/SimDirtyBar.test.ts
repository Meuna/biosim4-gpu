import { render, screen, fireEvent } from "@testing-library/svelte";
import { vi } from "vitest";
import SimDirtyBar from "./SimDirtyBar.svelte";

const defaultProps = {
    dirty: true,
    disabled: false,
    onRevert: () => {},
    onNextGen: () => {},
    onRewind: () => {},
};

describe("SimDirtyBar", () => {
    it("renders nothing when not dirty", () => {
        const { container } = render(SimDirtyBar, {
            ...defaultProps,
            dirty: false,
        });
        expect(container.querySelector(".dirty-bar")).toBeNull();
    });

    it("renders revert, next-gen and rewind buttons when dirty", () => {
        render(SimDirtyBar, { ...defaultProps });
        expect(
            screen.getByRole("button", { name: "Revert all changes" }),
        ).toBeTruthy();
        expect(
            screen.getByRole("button", { name: /advance one generation/i }),
        ).toBeTruthy();
        expect(screen.getByRole("button", { name: /rewind/i })).toBeTruthy();
    });

    it("calls onRevert when the revert button is clicked", async () => {
        const onRevert = vi.fn();
        render(SimDirtyBar, { ...defaultProps, onRevert });
        await fireEvent.click(
            screen.getByRole("button", { name: "Revert all changes" }),
        );
        expect(onRevert).toHaveBeenCalledOnce();
    });

    it("maps ctrl-click to autoPlay for next-gen and rewind", async () => {
        const onNextGen = vi.fn();
        const onRewind = vi.fn();
        render(SimDirtyBar, { ...defaultProps, onNextGen, onRewind });

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

    it("disables next-gen and rewind when disabled is true, revert stays enabled", () => {
        render(SimDirtyBar, { ...defaultProps, disabled: true });
        const nextGen = screen.getByRole("button", {
            name: /advance one generation/i,
        }) as HTMLButtonElement;
        const rewind = screen.getByRole("button", {
            name: /rewind/i,
        }) as HTMLButtonElement;
        const revert = screen.getByRole("button", {
            name: "Revert all changes",
        }) as HTMLButtonElement;
        expect(nextGen.disabled).toBe(true);
        expect(rewind.disabled).toBe(true);
        expect(revert.disabled).toBe(false);
    });
});
