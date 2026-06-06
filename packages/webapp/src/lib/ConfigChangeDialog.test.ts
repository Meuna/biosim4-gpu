import { render, screen, fireEvent } from "@testing-library/svelte";
import ConfigChangeDialog from "./ConfigChangeDialog.svelte";

const defaultProps = {
    open: true,
    onRevertContinue: () => {},
    onRewind: () => {},
};

describe("ConfigChangeDialog", () => {
    it("renders nothing when closed", () => {
        render(ConfigChangeDialog, { ...defaultProps, open: false });
        expect(screen.queryByRole("dialog")).toBeNull();
    });

    it("renders dialog with both options when open", () => {
        render(ConfigChangeDialog, defaultProps);
        expect(screen.getByRole("dialog")).toBeTruthy();
        expect(
            screen.getByRole("button", { name: /revert and resume/i }),
        ).toBeTruthy();
        expect(
            screen.getByRole("button", { name: /rewind with the new conf/i }),
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
            screen.getByRole("button", { name: /revert and resume/i }),
        );
        expect(called).toBe(true);
    });

    it("calls onRewind when that button is clicked", () => {
        let called = false;
        render(ConfigChangeDialog, {
            ...defaultProps,
            onRewind: () => {
                called = true;
            },
        });
        fireEvent.click(
            screen.getByRole("button", { name: /rewind with the new conf/i }),
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
