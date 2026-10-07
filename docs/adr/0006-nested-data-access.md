<!--
SPDX-License-Identifier: GPL-2.0-or-later
SPDX-FileCopyrightText: 2026 kio-unpack contributors
-->

# 0006. Data access for nested archives: never unpack more than necessary

Date: 2026-10-07
Status: Accepted

## Context

The project owner requires that a nested archive, or a file inside it, be served
without extracting the outer archive and without decoding more of it than the
position of the requested data needs.

Facts that bound what is possible:

- libarchive reads entries sequentially with cheap skipping, does not report
  where an entry's data lies, and cannot snapshot a decoder's state.
- libudfread gives random access to UDF files (ADR 0003); our partition, El
  Torito and optical-image parsers yield byte ranges and sector streams.
- ISO 9660 keeps its volume descriptors and directory records near the start of
  an image.
- zlib, liblzma, libbz2 and libzstd are in Fedora 44 and org.kde.Platform 6.10.
- In solid 7z and RAR blocks, extracting an entry decodes the block from its
  first file (`7zip/CPP/7zip/Archive/7z/7zExtract.cpp:263-264,325`).
- libarchive keeps 7z solid blocks ("folders") and the RAR solid flags in
  internal structures (`archive_read_support_format_7zip.c:174-202`,
  `archive_read_support_format_rar.c:67,84`, libarchive `8bb3bbdc`); neither
  `archive.h` nor `archive_entry.h` exposes solid-block membership or an
  entry's compression method.
- A worker can ask the user through `WorkerBase::messageBox()` with
  `WarningContinueCancel` and a "do not ask again" name
  (`kio/src/core/workerbase.h:269-277,340-345`); it returns 0 when no UI is
  available.
- 7-Zip offered in-place streams only for uncompressed containers, not for ZIP,
  7z or RAR (`bit7z/src/bitinputarchive.cpp:1099-1106`).

## Decision

Each nesting step takes the first applicable source:

1. **In-place random access** from the parent's reader: UDF files, partitions,
   El Torito images, optical-image tracks.
2. **Byte window** over data stored without compression, found by our **data
   locators**: an ISO 9660 directory walker (path to extents), a tar header walker
   (ustar, pax, GNU) and a ZIP central-directory reader (ZIP64 included). They
   locate data only; libarchive still builds the tree.
3. **Decoding on demand** (`DecodingByteSource`) for deflated ZIP entries and
   gzip (zlib, with checkpoints of decoder state so a backward seek restarts at
   the nearest checkpoint, as in zlib's `zran.c`), xz (liblzma, random access
   through the block index when the file has several blocks), bzip2 and zstd
   (forward only). An LRU block cache sits in front.
4. **Temporary file** only for entries in solid 7z/RAR blocks, 7z/RAR entries
   that need backward seeks, or when the backward-seek budget is exhausted.
   Temporary files are unnamed (`O_TMPFILE`), live below
   `QStandardPaths::CacheLocation` + `/kio-unpack`, and obey per-file and
   per-session size limits. Where the file system does not support
   `O_TMPFILE` (`EOPNOTSUPP`, for example on NFS (UNVERIFIED), or `EISDIR` on a kernel
   without it, open(2)), the file is created with `mkostemp()` and unlinked
   at once; only a crash between those two calls can leave it behind.

Because libarchive cannot tell which 7z or RAR entries share a solid block,
every entry of a 7z or RAR archive is treated as if it were in one. A
nested archive inside a 7z or RAR archive is presented as a file by default
(`[Nested] PresentSolidAs`, default `File`), whatever `PresentAs` says. Browsing
into one asks for confirmation with a `WarningContinueCancel` box stating the
data to decode and the temporary space, with "do not ask again", when the data
exceeds `[Nested] ConfirmCostlyAboveMiB` (default 64). Cancel fails the request
with `ERR_USER_CANCELED`; if no box can be shown, the worker proceeds and emits
the text as a KIO warning. Where Dolphin shows a worker's box is unknown
(OQ-6); spike S-3 at the start of M0b settles it before this confirmation is
built (`docs/roadmap.md`).

Counters in `UnpackStats` (bytes decoded, restarts, temporary bytes) let tests
prove that steps 1 to 3 never write temporary files. Each locator and the
checkpointing decoder has a fuzz harness.

## Consequences

ISO in ISO, ISO in tar, ISO in a stored or deflated ZIP entry, `disc.iso.gz` and
multi-block `disc.iso.xz` are browsed without temporary files, and deflated ZIP
and gzip get better random access than 7-Zip offered. Three locators and a
checkpointing decoder of untrusted data are ours to write, test and fuzz.
Budgets and checkpoint spacing are tuned in M2 (OQ-5).

## Alternatives considered

- **Temporary file for every nested archive:** simple, violates the requirement.
- **libarchive's filters for decoding:** no checkpoints, so every backward seek
  restarts at offset 0.
- **Re-reading through libarchive for each access:** works without locators but
  re-reads the directory and skips again on every backward seek; kept as the
  fallback when a locator cannot map an entry.
- **Refusing to browse solid-block entries, or extracting them silently:** the
  owner wants the option, with a warning.
- **Own 7z and RAR header readers to find the real solid blocks:** exact, but
  two more parsers of untrusted input, and 7z headers are usually
  LZMA-compressed (UNVERIFIED). Not in 1.0.
