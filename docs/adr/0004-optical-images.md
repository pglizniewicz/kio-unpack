<!--
SPDX-License-Identifier: GPL-2.0-or-later
SPDX-FileCopyrightText: 2026 kio-unpack contributors
-->

# 0004. Optical images: own parsers for CUE/BIN, raw dumps, MDS/MDF, CCD and TOC

Date: 2026-10-07
Status: Accepted

## Context

BIN/CUE is critical for the project owner, and every CD sector layout must work:
2048, 2056, 2324, 2332, 2336, 2340, 2352 and 2448 bytes, with Mode 1, Mode 2
Form 1, Mode 2 Form 2 and audio sectors.

libmirage (cdemu, GPL-2.0-or-later, about 44,000 lines of C) handles all these
layouts in its sector layer (`libmirage/mirage/sector.c`), but its CUE parser
knows only AUDIO, CDG, MODE1/2048, MODE1/2352, MODE2/2336, MODE2/2352, CDI/2336
and CDI/2352 (`images/image-cue/parser.c:225-233`), and its raw-dump parser only
2048, 2332, 2336 and 2352 (`images/image-iso/parser.c:100-104`). It reads no file
systems itself, only tracks and sectors (`mirage/utils.c:48-60`). It opens images
and companion files by name (`mirage/context.c:444-519`, `mirage/utils.c:125-150`),
so images inside archives would need temporary directories. It is not in Fedora,
had two memory-safety CVEs in 2019 (CVE-2019-15540, CVE-2019-15757), is not in
OSS-Fuzz, and aborts when its plugin directory is missing
(`mirage/mirage.c:167-170`); running it safely would need a separate,
seccomp-confined helper process with IPC per sector.

CUE/BIN, raw dumps, CCD/IMG/SUB and TOC/BIN are simple text or INI descriptors
over raw sectors; MDS/MDF is a reverse-engineered binary descriptor over raw
sectors. libmirage's license matches ours, so its parsers may be adapted with
attribution.

shared-mime-info 2.4 gives `.cue` files the type `application/x-cue` and has no
glob for `.bin`.

## Decision

- M3 delivers an optical-image layer in the worker: a sector-geometry model
  (sector size, user-data offset, subchannel, sector type) and parsers for
  CUE/BIN (all modes, including MODE2/2048 and data with 2448-byte sectors), raw
  dumps without a descriptor (sector size from the CD sync pattern and the
  position of `CD001`/`BEA01`), MDS/MDF, CCD/IMG/SUB and TOC/BIN.
- Companion files are looked up next to the descriptor in the same directory or
  archive directory, exact name first, then case-insensitively, so pairs inside
  archives need no temporary files. Descriptors written on Windows or DOS often
  store paths (`FILE "C:\GAMES\WAR2\WAR2.BIN" BINARY`,
  `FILE "DATA\TRACK01.BIN" BINARY`), and a descriptor from an untrusted source
  could name any file the user can read. Both `\` and `/` are separators; a
  relative path without `..` is tried below the descriptor's directory, then
  its last component; an absolute path, a drive letter, a UNC path or a path
  with `..` is reduced to its last component. Nothing outside the descriptor's
  directory and its subdirectories is ever opened (REQ-097).
- Each data track becomes a `TrackByteSource` read by libarchive or libudfread.
- Each audio track becomes a WAV file (16-bit stereo PCM at 44.1 kHz, byte order
  corrected for big-endian sources, subchannel removed). Audio-only discs list
  `Track NN.wav` at the root; mixed-mode discs list them under `[Volumes]`.
- CUE sheets of ripped albums: uncompressed PCM WAV files are mapped like BIN
  audio; compressed files (FLAC, MP3, ...) that each belong to one track are
  listed unchanged under their original names; one compressed file shared by
  several tracks is reported as not supported, because splitting it needs a
  decoder.
- Mode 2 Form 2 files are never delivered as 2048-byte pieces; until full support
  is decided (OQ-7) such a read fails with `ERR_CANNOT_READ`.
- Code adapted from libmirage keeps its copyright notices.

## Consequences

The formats that matter most work in-process, with every sector size, inside
archives, and without temporary files. Five descriptor parsers of untrusted input
are ours to test and fuzz. NRG, CDI, MDX, B6T, C2D, CIF, DAA, ISZ, ECM, CSO and CHD
are not supported in 1.0; libmirage remains a later idea for them, together with
splitting compressed albums and converting audio to FLAC or MP3.

## Alternatives considered

- **libmirage in a seccomp-confined helper process:** broad coverage, but gaps
  in exactly the CUE and raw-dump layouts that matter, IPC for every sector,
  temporary directories for nested images, and an unverified seccomp filter
  inside Flatpak's sandbox.
- **Patching libmirage upstream:** fixes the parser gaps but keeps the IPC and
  file-name constraints.
- **libcdio's image drivers:** GPL-3.0-or-later and fewer formats.
- **libsndfile for compressed album CUEs:** deferred with album splitting.
