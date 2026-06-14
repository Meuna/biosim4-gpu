#!/usr/bin/env bash
#
# Emit the webapp's snapshot-compatibility status relative to the last release
# tag: one of "unreleased", "compatible", or "diverged".
#
# Usage:
#   snapshot-compat-version.sh   # unreleased | compatible | diverged

set -euo pipefail

snap_header="packages/core/include/biosim/core/snapshot_defs.h"
io_header="packages/core/include/biosim/core/io_defs.h"
snap_macro="BIOSIM_SNAP_FORMAT_VERSION"
io_macro="BIOSIM_IO_SCHEMA_VERSION"

# Most recent release tag reachable from HEAD; empty when no tags exist yet.
latest_tag="$(git describe --tags --abbrev=0 --match 'v[0-9]*' 2>/dev/null || true)"

# Print the integer of a "#define <macro> <n>U" line read from stdin.
extract_macro() {
  awk -v macro="$1" \
    '$1 == "#define" && $2 == macro { gsub(/[^0-9]/, "", $3); print $3; exit }'
}

macro_at_tag() { # <tag> <header> <macro>
  git show "$1:$2" 2>/dev/null | extract_macro "$3" || true
}

macro_in_tree() { # <header> <macro>
  extract_macro "$2" <"$1"
}

if [ -z "$latest_tag" ]; then
  echo unreleased
  exit 0
fi

status=compatible
for pair in "$snap_header:$snap_macro" "$io_header:$io_macro"; do
  header="${pair%:*}"
  macro="${pair##*:}"
  tag_val="$(macro_at_tag "$latest_tag" "$header" "$macro")"
  tree_val="$(macro_in_tree "$header" "$macro")"
  # Unreadable at the tag, or a changed value, means the contract moved.
  if [ -z "$tag_val" ] || [ "$tag_val" != "$tree_val" ]; then
    status=diverged
    break
  fi
done
echo "$status"
