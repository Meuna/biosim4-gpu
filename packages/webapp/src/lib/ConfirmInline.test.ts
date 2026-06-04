import { render, screen, fireEvent } from "@testing-library/svelte";
import ConfirmInline from "./ConfirmInline.svelte";

const defaultProps = {
    open: true,
    onConfirm: () => {},
    onCancel: () => {},
};

describe("ConfirmInline", () => {
    it("renders nothing when closed", () => {
        render(ConfirmInline, { ...defaultProps, open: false });
        expect(screen.queryByRole("group")).toBeNull();
    });

    it("renders confirm and cancel buttons when open", () => {
        render(ConfirmInline, defaultProps);
        expect(screen.getByText("Yes")).toBeTruthy();
        expect(screen.getByText("No")).toBeTruthy();
    });

    it("renders custom labels", () => {
        render(ConfirmInline, {
            ...defaultProps,
            confirmLabel: "Delete",
            cancelLabel: "Keep",
        });
        expect(screen.getByText("Delete")).toBeTruthy();
        expect(screen.getByText("Keep")).toBeTruthy();
    });

    it("calls onConfirm when confirm button clicked", () => {
        let called = false;
        render(ConfirmInline, {
            ...defaultProps,
            onConfirm: () => {
                called = true;
            },
        });
        fireEvent.click(screen.getByText("Yes"));
        expect(called).toBe(true);
    });

    it("calls onCancel when cancel button clicked", () => {
        let called = false;
        render(ConfirmInline, {
            ...defaultProps,
            onCancel: () => {
                called = true;
            },
        });
        fireEvent.click(screen.getByText("No"));
        expect(called).toBe(true);
    });
});
