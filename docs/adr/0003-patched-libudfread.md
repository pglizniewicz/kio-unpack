<!--
SPDX-License-Identifier: GPL-2.0-or-later
SPDX-FileCopyrightText: 2026 kio-unpack contributors
-->

# 0003. UDF through a pinned, patched libudfread

Date: 2026-10-07
Status: Accepted

## Context

libudfread reads UDF from a caller-supplied block input
(`libudfread/src/udfread.h:147`, `src/blockinput.h:44-75`) and gives random access
to files (`udfread_file_seek`, `udfread_file_read`). It handles one type-1
partition and the UDF 2.50+ metadata partition with mirror fallback
(`src/udf_volume.c:552-600`, `src/udf_fs.c:173-229`); it rejects virtual (VAT) and
sparable partitions, fragmented UDF 2.60 metadata partitions and fragmented
directories (`src/udf_fs.c:716`). Compared with 7-Zip's UDF handler it lacks
timestamps, link counts and multiple logical volumes; neither reports
permissions or owners (`7zip/CPP/7zip/Archive/Udf/UdfHandler.cpp:228-255`).

libudfread decodes only length, type and allocation descriptors of a file entry
(`src/ecma167.h:229-248`), and its directory entry carries only type and a MUTF-8
name (`src/udfread.h:208-211`). UDF file entries record times, permissions,
owner and group at fixed offsets (ECMA-167 4/14.9 and 4/14.17), and symlink
targets as path component records.

Fedora 44 ships libudfread 1.2.0. Master (`b0bc695`, 2026-08-30) contains the
unreleased 1.3.0 fixes for invalid input: integer overflows, infinite loops on
bad descriptors, deleted-file handling (`ChangeLog`). libudfread is not part of
org.kde.Platform 6.10.

The project owner wants UDF timestamps and permissions in 1.0.

## Decision

- We extend libudfread's decoder for file entries and extended file entries and
  add an API that returns modification, access and creation times, permissions,
  owner, group, link count and symlink targets. The patch is offered upstream.
- kio-unpack builds libudfread itself, from commit `b0bc695` plus our patch:
  natively with `scripts/build-deps.sh` into a local prefix that needs no root, in the
  Flatpak as `flatpak/modules/libudfread.json` with a `patch` source. Both arrive
  in M2 with the UDF backend; the M1 Flatpak has no libudfread (ADR 0010). The
  Fedora package is not used.
- UDF is the first choice for the default volume of a disc (ADR 0008).

## Consequences

UDF listings show dates, permissions, owners and symlinks, and the robustness
fixes are in from the start. We carry a patch until upstream releases it and
rebase it when the pin moves. Packet-written discs (VAT, sparable) stay
unsupported, as they were with 7-Zip.

## Alternatives considered

- **Unpatched libudfread:** no dates or permissions.
- **libcdio's libudf:** has times and permissions, but is GPL-3.0-or-later, opens
  only file paths and ignores partition numbers, so Blu-ray metadata partitions
  fail (`libcdio/lib/udf/udf_fs.c:203,217,587-635`).
- **7-Zip's UDF handler:** dropped with 7-Zip (ADR 0002).
- **Own UDF reader:** full control, but more untrusted-input code; the fallback
  if upstream rejects the patch and carrying it becomes costly.
