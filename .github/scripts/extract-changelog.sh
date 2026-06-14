#!/usr/bin/env bash
#
# Print the changelog body for one version, to seed the (draft) GitHub release.
#
# Usage: extract-changelog.sh <version> [changelog-file]
set -euo pipefail

version="${1#v}" # the headings carry no leading "v"
changelog="${2:-CHANGELOG.md}"

notes=$(awk -v ver="[$version]" '
  $2 == ver  { grab = 1; next }
  /^## /     { grab = 0 }
  grab       { print }
' "$changelog")

echo "## Release Notes"
echo "${notes:-no changelog found for $version}"
