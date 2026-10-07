#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: 2026 kio-unpack contributors
#
# Generates the ISO 9660 test fixtures with xorriso. Called by the build
# (tests/CMakeLists.txt), never committed:
#
#     bash tests/fixtures/make-fixtures.sh <xorriso> <output-directory>
#
# Output:
#   rr-src/      the source tree; tests compare extracted bytes against it
#   rr.iso       Rock Ridge + Joliet (xorriso -as mkisofs -R -J), REQ-024
#   joliet.iso   Joliet only (-J --norock; xorriso writes Rock Ridge unless
#                told not to), REQ-025
#
# rr.iso fixes owner and group with -uid/-gid, so REQ-024's owner checks do
# not depend on who builds. Symlinks are never followed (no -f).

set -euo pipefail

if [ "$#" -ne 2 ]; then
    echo "usage: $0 <xorriso> <output-directory>" >&2
    exit 2
fi

XORRISO="$1"
OUT="$2"

SRC="$OUT/rr-src"
rm -rf "$SRC" "$OUT/rr.iso" "$OUT/joliet.iso"
mkdir -p "$SRC/dir" "$SRC/Dir_With_Long_Name"

printf 'Hello from inside an ISO image.\n' > "$SRC/dir/file.txt"
printf 'A long, mixed-case Rock Ridge and Joliet name.\n' \
    > "$SRC/Dir_With_Long_Name/A_Long_File_Name_With_MixedCase.txt"
printf 'Only the owner may read this.\n' > "$SRC/secret.txt"
chmod 0700 "$SRC/secret.txt"
ln -s dir/file.txt "$SRC/link.txt"
# Plain text without an extension: its MIME type comes from content (REQ-029).
printf 'This file has no extension, so its type comes from its bytes.\n' > "$SRC/noext"
# 3 MiB of deterministic, non-repeating text, larger than one data chunk of
# get() for any chunk size up to 1 MiB (REQ-030).
seq 1 1000000 > "$SRC/big.bin"
truncate -s 3145728 "$SRC/big.bin"

chmod 0755 "$SRC/dir" "$SRC/Dir_With_Long_Name"
chmod 0644 "$SRC/dir/file.txt" "$SRC/Dir_With_Long_Name/A_Long_File_Name_With_MixedCase.txt" "$SRC/noext" "$SRC/big.bin"

"$XORRISO" -as mkisofs -quiet -R -J -uid 1234 -gid 5678 -V KIO_UNPACK_RR \
    -o "$OUT/rr.iso" "$SRC"
"$XORRISO" -as mkisofs -quiet -J --norock -V KIO_UNPACK_JOLIET \
    -o "$OUT/joliet.iso" "$SRC"
