#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: 2026 kio-unpack contributors
#
# Smoke test from ctest: kioclient --platform offscreen against the worker in
# the build tree (QT_PLUGIN_PATH is set by the test), and kio-unpack-cli
# against the same fixtures.
#
#     bash tests/smoke/smoke.sh <kioclient> <kio-unpack-cli> <fixtures-dir>

set -euo pipefail

if [ "$#" -ne 3 ]; then
    echo "usage: $0 <kioclient> <kio-unpack-cli> <fixtures-dir>" >&2
    exit 2
fi

KIOCLIENT="$1"
CLI="$2"
FIXTURES="$3"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

fail() {
    echo "FAIL: $*" >&2
    exit 1
}

kc() {
    "$KIOCLIENT" --platform offscreen "$@"
}

# Names listed by kioclient, without "." / ".." and the trailing empty line.
kc_names() {
    kc ls "$1" | grep -v -x -e '' -e '.' -e '..' | LC_ALL=C sort
}

RR="$FIXTURES/rr.iso"

# Discovery and stat of the image root (REQ-001, REQ-023).
kc stat "unpack:$RR" > "$WORK/stat.txt" || fail "kioclient stat unpack:$RR"
grep -q -E '^FILE_TYPE +0040000$' "$WORK/stat.txt" || fail "image root is not a directory"
grep -q -E '^MIME_TYPE +application/vnd\.efi\.iso$' "$WORK/stat.txt" || fail "image root has the wrong MIME type"

# Listing (REQ-008).
kc_names "unpack:$RR" > "$WORK/kioclient-ls.txt" || fail "kioclient ls unpack:$RR"
grep -q -x 'dir' "$WORK/kioclient-ls.txt" || fail "dir missing from the listing"
grep -q -x 'big.bin' "$WORK/kioclient-ls.txt" || fail "big.bin missing from the listing"

# Reading (REQ-030).
kc cat "unpack:$RR/dir/file.txt" > "$WORK/file.txt" || fail "kioclient cat file.txt"
cmp "$WORK/file.txt" "$FIXTURES/rr-src/dir/file.txt" || fail "file.txt differs from its source"
kc cat "unpack:$RR/big.bin" > "$WORK/big-kioclient.bin" || fail "kioclient cat big.bin"
cmp "$WORK/big-kioclient.bin" "$FIXTURES/rr-src/big.bin" || fail "big.bin from kioclient differs from its source"

# Joliet-only image listed with its Joliet names (REQ-025).
kc_names "unpack:$FIXTURES/joliet.iso" > "$WORK/joliet-ls.txt" || fail "kioclient ls joliet.iso"
grep -q -x 'Dir_With_Long_Name' "$WORK/joliet-ls.txt" || fail "Joliet name missing"

# kio-unpack-cli reports the same entries and bytes as the worker (REQ-103).
"$CLI" list "$RR" | LC_ALL=C sort > "$WORK/cli-ls.txt" || fail "kio-unpack-cli list"
diff -u "$WORK/kioclient-ls.txt" "$WORK/cli-ls.txt" || fail "kio-unpack-cli list differs from kioclient ls"
"$CLI" cat "$RR/big.bin" > "$WORK/big-cli.bin" || fail "kio-unpack-cli cat big.bin"
cmp "$WORK/big-cli.bin" "$WORK/big-kioclient.bin" || fail "kio-unpack-cli cat differs from kioclient cat"

echo "smoke tests passed"
