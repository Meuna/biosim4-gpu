import { render, screen, fireEvent } from "@testing-library/svelte";
import ConfigChangeDialog from "./ConfigChangeDialog.svelte";

const defaultProps = {
    open: true,
    onRevertContinue: () => {},
    onPauseSaveGenome: () => {},
    onPauseClearGenome: () => {},
};

describe("ConfigChangeDialog", () => {
    it("renders nothing when closed", () => {
        render(ConfigChangeDialog, { ...defaultProps, open: false });
        expect(screen.queryByRole("dialog")).toBeNull();
    });

    it("renders dialog with all three options when open", () => {
        render(ConfigChangeDialog, defaultProps);
        expect(screen.getByRole("dialog")).toBeTruthy();
        expect(
            screen.getByRole("button", { name: /revert and continue/i }),
        ).toBeTruthy();
        expect(
            screen.getByRole("button", { name: /pause and save genome/i }),
        ).toBeTruthy();
        expect(
            screen.getByRole("button", { name: /pause and clear genome/i }),
        ).toBeTruthy();
    });

    it("calls onRevertContinue when that button is clicked", () => {
        let called = false;
        render(ConfigChangeDialog, {
            ...defaultProps,
            onRevertContinue: () => {
                called = true;
            },
        });
        fireEvent.click(
            screen.getByRole("button", { name: /revert and continue/i }),
        );
        expect(called).toBe(true);
    });

    it("calls onPauseSaveGenome when that button is clicked", () => {
        let called = false;
        render(ConfigChangeDialog, {
            ...defaultProps,
            onPauseSaveGenome: () => {
                called = true;
            },
        });
        fireEvent.click(
            screen.getByRole("button", { name: /pause and save genome/i }),
        );
        expect(called).toBe(true);
    });

    it("calls onPauseClearGenome when that button is clicked", () => {
        let called = false;
        render(ConfigChangeDialog, {
            ...defaultProps,
            onPauseClearGenome: () => {
                called = true;
            },
        });
        fireEvent.click(
            screen.getByRole("button", { name: /pause and clear genome/i }),
        );
        expect(called).toBe(true);
    });

    it("calls onRevertContinue on Escape keydown", async () => {
        let called = false;
        render(ConfigChangeDialog, {
            ...defaultProps,
            onRevertContinue: () => {
                called = true;
            },
        });
        await fireEvent.keyDown(window, { key: "Escape" });
        expect(called).toBe(true);
    });
});
