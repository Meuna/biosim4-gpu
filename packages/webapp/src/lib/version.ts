// Version label shown in the TopBar. The values come from build-time env vars
// set by the Pages workflow (.github/workflows/pages.yml):
//   VITE_WEBAPP_VERSION  — `git describe --tags --always --dirty`, the version
//                          string this build was made from.
//   VITE_SNAPSHOT_COMPAT — "compatible" | "diverged" | "unreleased" from
//                          `.github/scripts/snapshot-compat-version.sh`: whether
//                          the snapshot format/schema still matches the last
//                          native release.
// Outside the workflow (`bun run dev|test`) the vars are unset; we fall back to
// "dev" / "unreleased".

/**
 * Returns a human note when the snapshot contract has diverged from `version`,
 * or null when the build is still snapshot-compatible with the last release
 * (or no release exists yet to compare against).
 */
export function snapshotCompatNote(
    version: string,
    compat: string,
): string | null {
    if (compat !== "diverged") {
        return null;
    }
    return `The web app ${version} embed snapshot format changes ahead of the native release — snapshots may be incompatible`;
}

export const webappVersion = import.meta.env.VITE_WEBAPP_VERSION ?? "dev";
export const snapshotCompatNoteText = snapshotCompatNote(
    webappVersion,
    import.meta.env.VITE_SNAPSHOT_COMPAT ?? "unreleased",
);
