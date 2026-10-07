#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: 2026 kio-unpack contributors
#
# REQ-087: no hardcoded installation prefix in code, build files, scripts or
# CI; locations come from CMake installation variables. Documentation and the
# upstream Flatpak files are not checked: they describe systems, not our build.
#
#     bash tests/check-no-usr-paths.sh <source-dir>

set -euo pipefail

if [ "$#" -ne 1 ]; then
    echo "usage: $0 <source-dir>" >&2
    exit 2
fi

cd "$1"

# Assembled from two parts so that this script does not match itself.
prefix='/u''sr'

targets=()
for candidate in src tests prototypes scripts .github CMakeLists.txt; do
    if [ -e "$candidate" ]; then
        targets+=("$candidate")
    fi
done

# A "#!" line at the top of a script names the interpreter, not an
# installation path, and is the portable way to do so; it is not checked.
matches="$(grep -r -n -I -E "${prefix}(/|\\b)" "${targets[@]}" | grep -v -E '^[^:]+:1:#!' || true)"
if [ -n "$matches" ]; then
    printf '%s\n' "$matches"
    echo "FAIL: hardcoded ${prefix} path found (REQ-087)" >&2
    exit 1
fi
echo "no hardcoded ${prefix} paths"
