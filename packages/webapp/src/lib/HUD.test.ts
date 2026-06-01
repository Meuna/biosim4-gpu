import { render, screen } from "@testing-library/svelte";
import HUD from "./HUD.svelte";

describe("HUD", () => {
    it("renders sparkline SVG when survival history has entries", () => {
        render(HUD, { survivalHistory: [0.4, 0.6, 0.7] });
        expect(screen.getByLabelText("Survival rate sparkline")).toBeTruthy();
    });

    it("shows dash placeholder when survival history is empty", () => {
        render(HUD, { survivalHistory: [] });
        expect(screen.queryByLabelText("Survival rate sparkline")).toBeNull();
    });

    it("labels the aside as Survival history", () => {
        render(HUD, { survivalHistory: [] });
        expect(screen.getByLabelText("Survival history")).toBeTruthy();
    });
});
