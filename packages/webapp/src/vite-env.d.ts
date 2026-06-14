/// <reference types="vite/client" />

// Build-time env vars exposed via import.meta.env. Set by the Pages workflow
// (.github/workflows/pages.yml); unset under `bun run dev|test`.
interface ImportMetaEnv {
    readonly VITE_WEBAPP_VERSION?: string;
    readonly VITE_SNAPSHOT_COMPAT?: string;
}

interface ImportMeta {
    readonly env: ImportMetaEnv;
}
