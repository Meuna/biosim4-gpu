import { classifyDroppedFiles, downloadBlob, pickFile } from "./fileTransfer";

function file(name: string): File {
    return new File(["x"], name, { type: "text/plain" });
}

describe("classifyDroppedFiles", () => {
    it("routes a .toml to `toml` and anything else to `snap`", () => {
        const result = classifyDroppedFiles([
            file("config.toml"),
            file("pop.snap"),
        ]);
        expect(result.toml?.name).toBe("config.toml");
        expect(result.snap?.name).toBe("pop.snap");
        expect(result.error).toBeNull();
    });

    it("leaves the absent slot null for a single file", () => {
        const onlyToml = classifyDroppedFiles([file("config.toml")]);
        expect(onlyToml.toml?.name).toBe("config.toml");
        expect(onlyToml.snap).toBeNull();

        const onlySnap = classifyDroppedFiles([file("pop.snap")]);
        expect(onlySnap.toml).toBeNull();
        expect(onlySnap.snap?.name).toBe("pop.snap");
    });

    it("rejects more than two files with an error and no routing", () => {
        const result = classifyDroppedFiles([
            file("a.toml"),
            file("b.snap"),
            file("c.snap"),
        ]);
        expect(result.toml).toBeNull();
        expect(result.snap).toBeNull();
        expect(result.error).toMatch(/at most 2/);
    });
});

describe("pickFile", () => {
    // Capture the transient input pickFile creates so the test can drive its
    // change / cancel events. jsdom's click() on a file input is a no-op, so it
    // neither opens a dialog nor throws.
    function withCapturedInput(): HTMLInputElement[] {
        const created: HTMLInputElement[] = [];
        const real = document.createElement.bind(document);
        vi.spyOn(document, "createElement").mockImplementation(
            (tag: string) => {
                const el = real(tag);
                if (tag === "input") created.push(el as HTMLInputElement);
                return el;
            },
        );
        return created;
    }

    it("resolves with the chosen file and honours `accept`", async () => {
        const created = withCapturedInput();
        const promise = pickFile(".toml");
        const input = created[0];
        expect(input.accept).toBe(".toml");
        const chosen = file("config.toml");
        Object.defineProperty(input, "files", {
            value: [chosen],
            configurable: true,
        });
        input.dispatchEvent(new Event("change"));
        expect(await promise).toBe(chosen);
    });

    it("resolves null when the picker is cancelled", async () => {
        const created = withCapturedInput();
        const promise = pickFile(".snap");
        created[0].dispatchEvent(new Event("cancel"));
        expect(await promise).toBeNull();
    });
});

describe("downloadBlob", () => {
    it("downloads via an anchor and revokes the object URL", () => {
        // jsdom implements neither createObjectURL nor revokeObjectURL, so
        // install stubs directly rather than spying on missing methods.
        const createSpy = vi.fn().mockReturnValue("blob:stub");
        const revokeSpy = vi.fn();
        URL.createObjectURL = createSpy;
        URL.revokeObjectURL = revokeSpy;
        const clickSpy = vi
            .spyOn(HTMLAnchorElement.prototype, "click")
            .mockImplementation(() => {});

        downloadBlob("out.toml", "hello", "text/plain");

        expect(createSpy).toHaveBeenCalledOnce();
        expect(clickSpy).toHaveBeenCalledOnce();
        expect(revokeSpy).toHaveBeenCalledWith("blob:stub");
    });
});
