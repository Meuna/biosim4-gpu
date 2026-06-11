// Browser file-transfer helpers shared by the config/snapshot import-export
// flows. They concentrate the DOM mechanics (transient <input>, anchor
// download) in one place so `App.svelte` keeps only orchestration, and so the
// click and drag-drop entry paths converge on a single "load this File" flow.

// Opens a native file picker and resolves with the chosen file, or `null` if
// the user cancels. Uses a transient (never-mounted) <input>, so callers no
// longer keep a hidden element + `bind:this` + an `onchange` handler around.
// The `cancel` event covers the dismissed-picker path.
export function pickFile(accept: string): Promise<File | null> {
    return new Promise((resolve) => {
        const input = document.createElement("input");
        input.type = "file";
        input.accept = accept;
        const settle = (file: File | null): void => {
            input.removeEventListener("change", onChange);
            input.removeEventListener("cancel", onCancel);
            resolve(file);
        };
        const onChange = (): void => settle(input.files?.[0] ?? null);
        const onCancel = (): void => settle(null);
        input.addEventListener("change", onChange);
        input.addEventListener("cancel", onCancel);
        input.click();
    });
}

// Triggers a browser download of `data` as `filename`. Wraps the
// create-anchor / click / revoke-object-URL dance shared by the config-TOML and
// snapshot exports.
export function downloadBlob(
    filename: string,
    data: BlobPart,
    mime: string,
): void {
    const url = URL.createObjectURL(new Blob([data], { type: mime }));
    const a = document.createElement("a");
    a.href = url;
    a.download = filename;
    a.click();
    URL.revokeObjectURL(url);
}

// The outcome of routing a drop: at most one TOML config and one snapshot, or
// an error message when too many files were dropped.
export interface DroppedFiles {
    toml: File | null;
    snap: File | null;
    error: string | null;
}

// Routes dropped files into a TOML config (`.toml`) and a snapshot (anything
// else), enforcing the "at most 2 files" rule. Pure — the caller owns the
// reads and error reporting.
export function classifyDroppedFiles(files: File[]): DroppedFiles {
    if (files.length > 2) {
        return {
            toml: null,
            snap: null,
            error: "Drop at most 2 files (one .toml, one snapshot)",
        };
    }
    const toml = files.find((f) => f.name.endsWith(".toml")) ?? null;
    const snap = files.find((f) => !f.name.endsWith(".toml")) ?? null;
    return { toml, snap, error: null };
}
