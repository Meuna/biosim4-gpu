import { render, screen, fireEvent } from "@testing-library/svelte";
import { vi } from "vitest";
import FormFactorControl from "./FormFactorControl.svelte";

describe("FormFactorControl", () => {
    it("shows the three form-factor pills", () => {
        render(FormFactorControl, {
            props: { value: "desktop", onchange: vi.fn() },
        });
        expect(screen.getByRole("button", { name: "Desktop" })).toBeTruthy();
        expect(screen.getByRole("button", { name: "Tablet" })).toBeTruthy();
        expect(screen.getByRole("button", { name: "Phone" })).toBeTruthy();
    });

    it("marks only the active pill as pressed", () => {
        render(FormFactorControl, {
            props: { value: "tablet", onchange: vi.fn() },
        });
        expect(
            screen
                .getByRole("button", { name: "Tablet" })
                .getAttribute("aria-pressed"),
        ).toBe("true");
        expect(
            screen
                .getByRole("button", { name: "Desktop" })
                .getAttribute("aria-pressed"),
        ).toBe("false");
    });

    it("marks no pill pressed when value is null", () => {
        render(FormFactorControl, {
            props: { value: null, onchange: vi.fn() },
        });
        for (const name of ["Desktop", "Tablet", "Phone"]) {
            expect(
                screen
                    .getByRole("button", { name })
                    .getAttribute("aria-pressed"),
            ).toBe("false");
        }
    });

    it("calls onchange with the form factor when a pill is clicked", async () => {
        const onchange = vi.fn();
        render(FormFactorControl, { props: { value: "desktop", onchange } });
        await fireEvent.click(screen.getByRole("button", { name: "Phone" }));
        expect(onchange).toHaveBeenCalledWith("phone");
    });

    it("disables every pill when disabled", () => {
        render(FormFactorControl, {
            props: { value: "desktop", disabled: true, onchange: vi.fn() },
        });
        for (const name of ["Desktop", "Tablet", "Phone"]) {
            expect(
                (screen.getByRole("button", { name }) as HTMLButtonElement)
                    .disabled,
            ).toBe(true);
        }
    });
});
