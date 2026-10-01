#!/usr/bin/env bash
#
# Export one module panel, light and dark versions: convert the text of
# res/staging/<panel>[-dark].svg to paths, drop its components layer (a
# positioning aid: Rack draws those widgets from res/components) and write the
# result to res/<panel>[-dark].svg. Requires Inkscape.
#
# Usage: res/staging/export-panel.sh <panel>
#
# where <panel> is the SVG file name without extension, with or without the
# "-dark" suffix (e.g., SP4TStereoIn). Usually run as `pixi run export-panel
# <panel>`.

set -euo pipefail

if [ $# -ne 1 ]; then
	echo "usage: $(basename "$0") <panel>" >&2
	exit 1
fi

# Paths are relative to the repository root, whatever the current directory.
cd "$(dirname "$0")/../.."

# Makes the fonts shipped in res/staging/fonts/ (used by the panels but not
# installed by default) available to Inkscape.
export FONTCONFIG_FILE="$PWD/res/staging/fonts/fonts.conf"

panel="${1%-dark}"

for version in "$panel" "$panel-dark"; do
	src="res/staging/$version.svg"
	if [ ! -f "$src" ]; then
		echo "no such panel: $src" >&2
		exit 1
	fi
	inkscape "$src" \
		--actions="select-by-id:components;delete-selection" \
		--export-text-to-path \
		--export-plain-svg \
		--export-overwrite \
		--export-filename="res/$version.svg"
done
