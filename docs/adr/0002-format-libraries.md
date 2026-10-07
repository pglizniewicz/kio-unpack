<!--
SPDX-License-Identifier: GPL-2.0-or-later
SPDX-FileCopyrightText: 2026 kio-unpack contributors
-->

# 0002. Format libraries: libarchive, patched libudfread and own parsers

Date: 2026-10-07
Status: Accepted

## Context

No single library reads every format kio-unpack targets, and each candidate has
a different access model, license and packaging situation. Release 1.0 covers
ISO 9660, UDF, partitions, El Torito, the common archive formats, single-stream
compression, nested archives, and the optical image formats CUE/BIN, raw dumps,
MDS/MDF, CCD and TOC (`docs/roadmap.md`).

- **libarchive** (BSD) is in Fedora 44 (3.8.7) and in org.kde.Platform 6.10
  (3.8.9). It reads 7z, ar, cab, cpio, ISO 9660, LHA, mtree, RAR, RAR5, tar, WARC,
  XAR and ZIP (`libarchive/archive_read_support_format_all.c`). It is a streaming
  reader but accepts seek and skip callbacks (`libarchive/archive.h:528-556`). Its
  ISO 9660 reader supports Rock Ridge and Joliet, prefers Rock Ridge when both
  are present (`archive_read_support_format_iso9660.c:1187-1197`; its
  libarchive-formats(5) page says the opposite and is out of date), and both
  can be disabled with the `iso9660:!rockridge` / `!joliet` options
  (archive_read_set_options(3); checked with libarchive 3.8.7,
  `docs/architecture.md`, section 4.4). It does not report where an entry's data lies,
  and it is covered by OSS-Fuzz. The Flathub Dolphin manifest builds Ark, which
  requires `LibArchive 3.3.3` (`ark/CMakeLists.txt:84`), without a libarchive
  module, which suggests org.kde.Sdk carries its headers (checked in M1 step 0).
- **7-Zip's `7z.so`** reads many more formats (Fedora's build has 61 handlers,
  `7z i`), but only through COM-style C++ interfaces. Its ISO handler prefers the
  Joliet tree and reads Rock Ridge only partially
  (`7zip/CPP/7zip/Archive/Iso/IsoIn.cpp:658-663`). The wrapper **bit7z** 4.1.0
  (MPL-2.0) is in no Linux distribution (Repology lists only vcpkg), is static
  only, needs the 7-Zip sources at build time and downloads CPM.cmake
  (`bit7z/cmake/Dependencies.cmake:13-21`), hides in-place item streams behind a
  protected member (`bit7z/include/bit7z/bitinputarchive.hpp:675-689`), and
  misdetects isohybrid images (`bit7z/src/formatdetect.cpp:446,462-492`).
- **libudfread** (LGPL-2.0-or-later, VideoLAN) reads UDF up to 2.60 including
  metadata partitions with mirror fallback, from a caller-supplied block input
  (`libudfread/src/udfread.h:147`), with random access to files. Its API reports
  no times, permissions or symlinks (ADR 0003).
- Replacement libraries for the formats only 7-Zip covers were checked: libyal
  (HFS+, DMG, VHD, VMDK, QCOW, NTFS, APFS) is not in Fedora and declares itself
  experimental or alpha; wimlib is GPL-3.0-or-later in Fedora; squashfs-tools-ng
  is LGPL-3.0-or-later; libcdio's libudf is GPL-3.0-or-later with path-only input
  and one partition; The Sleuth Kit is under GPL-incompatible CPL/IPL.
- **libmirage** (GPL-2.0-or-later, cdemu) turns many optical image formats into
  tracks and sectors. It is not in Fedora, its CUE and raw-dump parsers miss
  several sector sizes, and it opens images and their companions by file name
  (ADR 0004).
- CLI tools (unar/lsar, Aaru, deark) would mean running external programs.

## Decision

- **libarchive** is the archive backend from M0 and the ISO 9660 reader (Rock
  Ridge as the default name flavour, Joliet as an alternate).
- **libudfread**, pinned and patched by us, reads UDF from M2 (ADR 0003).
- **Our own parsers** cover what no suitable library offers: MBR/GPT/APM
  partition tables and the El Torito boot catalog (M2), data locators for ISO
  9660, tar and ZIP (M2, ADR 0006), and the optical image formats (M3, ADR 0004).
- **zlib, liblzma, libbz2 and libzstd** decode compressed streams (ADR 0006).
- **Format choice is content-based and deterministic**: our own sniffing and a
  fixed priority table decide; a failed open falls through to the next backend.
- **Not used in 1.0:** 7-Zip, bit7z, libmirage and CLI-wrapping backends. HFS and
  HFS+, DMG, SquashFS, WIM, virtual-disk formats, NRG, CDI, MDX, DAA, ISZ, ECM,
  CSO and CHD are listed in `docs/roadmap.md` as later ideas.

## Consequences

All runtime libraries except libudfread come from the distribution (Fedora on
the development machine) and the KDE runtime; libudfread is built from a pinned
commit in both builds. Linux ISOs show correct
Rock Ridge names, permissions and symlinks from M0. Formats outside the list
report "not supported" with `ERR_WORKER_DEFINED`. The own parsers read untrusted
data and each needs tests and a fuzz harness.

## Alternatives considered

- **7-Zip through bit7z** or through an own thin wrapper over its interfaces:
  widest coverage, but weaker ISO 9660, vendoring of bit7z and the 7-Zip sources,
  and a COM-style API whose in-place streams bit7z hides. Rejected by the owner.
- **libmirage for optical images:** see ADR 0004.
- **Per-format replacement libraries for 7-Zip's formats:** experimental,
  missing from Fedora, or licensed GPL-3/LGPL-3, which would change the
  effective license; deferred with those formats.
- **KArchive** (kio-extras) has no disc-image support; **Krusader's bundled
  libisofs 0.2** is an old ISO 9660-only copy.
