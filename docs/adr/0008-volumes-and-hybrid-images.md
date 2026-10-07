<!--
SPDX-License-Identifier: GPL-2.0-or-later
SPDX-FileCopyrightText: 2026 kio-unpack contributors
-->

# 0008. Volumes and hybrid images

Date: 2026-10-07
Status: Accepted, except the representation of `[Volumes]` as a directory with a
link target whose `get` returns text, which is Proposed until the prototype of
spike S-4 at the start of M0b (OQ-11, `docs/roadmap.md`)

## Context

Many images hold more than one file system or view: DVDs carry ISO 9660 and UDF,
hybrid Mac/PC CDs carry ISO 9660 and HFS or HFS+, bootable discs carry El Torito
images, and mixed-mode or multi-session discs carry several tracks. Rock Ridge
and Joliet are two name sets for the same files, not separate file systems, and
nearly every ISO has a Joliet descriptor.

KIO cannot tell a copy from browsing (ADR 0005), so a plain directory
`[Volumes]` at the image root would be copied with everything else: copying a
DVD with UDF, Rock Ridge and Joliet would write the same files four or five
times. `CopyJob` lists sources with `listRecursive`
(`kio/src/core/copyjob.cpp:1204`, `startListing`), whose default flags include
hidden entries (`listjob.h:169`); the only hidden filter tests for a leading dot
(`listjob.cpp:178`) and nothing in `ListJob` or `CopyJob` reads `UDS_HIDDEN`.
The recursion skips links (`listjob.cpp:109`, `maybeRecurse`:
`if (!entry.isDir() || entry.isLink())`), and `UDSEntry::isLink()` only tests
for a non-empty `UDS_LINK_DEST` while `isDir()` only tests `UDS_FILE_TYPE`
(`udsentry.cpp:506-514`). `CopyJob` queues a listed directory that has a link
target as a file (`copyjob.cpp:988-994`, `addCopyInfoFromUDSEntry`), does not
list a source URL that is a link (`copyjob.cpp:699-701`, `sourceStated`: "treat
symlinks as files (no recursion)"), and, when source and destination protocols
differ, copies such an entry with `KIO::file_copy`, that is through `get`
(`copyjob.cpp:2206-2234`, `processCopyNextFile`). `KFileItem` opens an item at
`UDS_TARGET_URL` when set and otherwise at its own URL, never at
`UDS_LINK_DEST` (`kfileitem.cpp:1794-1806`); a non-empty `UDS_LINK_DEST` makes
the item a link (`kfileitem.cpp:376`) and adds the `emblem-symbolic-link`
overlay (`kfileitem.cpp:1331-1332`). kio_file reports a symlink to a directory
with `UDS_LINK_DEST` and, when the stat details ask to resolve symlinks, with
the target's type, because it re-stats the target into the same buffer
(`kio/src/kioworkers/file/file_unix.cpp:199-245`), so a directory with a link
target is an existing KIO convention. All KIO citations are at commit
`69f11e2c`.

Krusader exposes every volume descriptor as a top-level virtual folder
(`krusader/plugins/iso/kiso.cpp:464-503`). With the patched libudfread (ADR 0003)
UDF gives names, sizes, times, permissions and symlinks, and unlike ISO 9660 it
holds files of 4 GiB or more and long names. No library in 1.0 reads HFS or HFS+
(ADR 0002).

## Decision

- The image root shows the **default volume**, in the order UDF, ISO 9660 with
  Rock Ridge, ISO 9660 with Joliet, plain ISO 9660.
- When an image has any other volume or name flavour, the root also lists one
  reserved folder `[Volumes]` whose children are all volumes by label
  (`ISO9660`, `ISO9660 (Joliet)`, `ISO9660 (Rock Ridge)`, `UDF`, `HFS`, `HFS+`,
  `El Torito`, `Session 2`, audio tracks of mixed-mode discs), the default one
  included. A real entry named `[Volumes]` wins; the virtual folder then becomes
  `[Volumes~1]`, `[Volumes~2]`, and so on. `[Volumes]` exists only at an image
  root, never inside a volume reached through it.
- `[Volumes]` is reported as a directory that is also a symbolic link
  (`S_IFDIR` with `UDS_LINK_DEST`), so that copying the image root copies the
  default volume once. `get` on `[Volumes]` returns a short text that names the
  volumes; copying the image root to disk leaves that text as one small file
  named `[Volumes]`. `stat` keeps reporting `inode/directory` while `get`
  returns `text/plain`; the mismatch is deliberate.
- Audio-only discs list their WAV tracks at the root, without `[Volumes]`
  (ADR 0004).
- HFS and HFS+ volumes are listed under `[Volumes]` with the display type
  "(not supported)" and report `ERR_WORKER_DEFINED` when entered. An HFS
  volume found both through an Apple partition map entry and through the
  signature at offset 0x400 is listed once.
- An isohybrid image is a disc image. A partition of it is listed under
  `[Volumes]` only when it is neither the ISO 9660 area itself nor an El Torito
  boot image, which `El Torito` already lists.
- Volume selection is part of the path; no query or fragment is used.

## Consequences

Ordinary ISOs and DVDs look like a plain folder with stable paths, as `zip:/`
does, and alternate sides stay reachable without special syntax. The bracketed
name may surprise users once; its display type and Dolphin's link emblem mark
it as virtual. Copying the image root, or everything selected in it, copies the
default volume once plus the small `[Volumes]` text file; a specific volume is
copied from inside `[Volumes]`. Selecting `[Volumes]` itself and copying it
also gives only the text file. The KIO code paths above are read from source,
not run; spike S-4 at the start of M0b is a prototype worker that checks the
whole behaviour in Dolphin and through the KIO API, records the result in
`docs/prototypes/m0b-volumes-link.md` and picks the `UDS_LINK_DEST` value
(OQ-11). Only then is this part accepted. If it fails, the fallback is the
alternative "only distinct content plus a context-menu action" below. The Mac side of hybrid discs is
visible but unreadable in 1.0.

## Alternatives considered

- **Always-virtual root (Krusader):** every ordinary ISO would need an extra
  click and get unusual paths.
- **Virtual folders only for multi-volume images:** the Joliet descriptor on
  almost every ISO would push most images into the virtual layout, and the root
  layout would change between images.
- **`[Volumes]` as a plain directory:** simplest, but every recursive copy of
  the image root duplicates the data once per volume and name flavour.
- **`[Volumes]` marked `UDS_HIDDEN`:** Dolphin would not select it with hidden
  files off, but `CopyJob` ignores the flag, so copying the image root still
  duplicates the data.
- **`[Volumes]` not listed, reachable only by typing its path or through a
  context-menu action:** copy-safe, but users would have to guess that an image
  has other volumes.
- **Only distinct content (El Torito, HFS, sessions, tracks, partitions) under a
  plain `[Volumes]`, name flavours and the ISO side of UDF discs through a
  context-menu action:** copy-safe for most images, but on an ordinary ISO with
  Joliet nothing in the listing shows that another name set exists. Kept as
  the fallback if the prototype for OQ-11 fails.
- **ISO 9660 with Rock Ridge before UDF:** was considered while libudfread
  reported no metadata; with the patch, UDF's large-file and long-name support
  wins.
