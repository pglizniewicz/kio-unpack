<!--
SPDX-License-Identifier: GPL-2.0-or-later
SPDX-FileCopyrightText: 2026 kio-unpack contributors
-->

# 0005. URL layout and presentation of nested archives

Date: 2026-10-07
Status: Accepted

## Context

Dolphin hands a local archive to a KIO worker by replacing only the scheme of its
URL (`dolphin/src/views/dolphinview.cpp:1852-1878`), so the worker receives
`unpack:/home/u/disc.iso` and must split longer paths into the on-disk image and
the path inside it. kio-extras does this by stat'ing each path prefix until it
finds a non-directory (`kio-extras/archive/kio_archivebase.cpp:84-131`); its cache
check compares string prefixes without a separator boundary (`:58-69`). Krusader's
iso worker selects sessions with a URL fragment
(`krusader/plugins/iso/iso.cpp:208`).

Dolphin rewrites only local files, so an entry inside an `unpack:` listing can be
entered by double-click only if the worker reports it as a directory. Total
Commander shows nested archives as browsable items with an archive icon. KIO cannot
tell a copy from browsing: `CopyJob` stats sources with the same source-side
request (`kio/src/core/copyjob.cpp:1130`, `statjob.cpp:203-204`) that Dolphin uses
for display (`dolphin/src/dolphinviewcontainer.cpp:888`), so an entry is either a
directory or a file for every operation. Dolphin treats a location as a file only
when listing it fails with `ERR_IS_FILE`
(`dolphin/src/kitemviews/kfileitemmodel.cpp:3355-3357`). Service menus can be
restricted to a protocol, a URL count and MIME types
(`kio/src/widgets/kfileitemactions.cpp:620-677`), and `KIO::file_copy` reads a
non-local source with `get` without listing it.

## Decision

- **URL:** `unpack:` + absolute image path + optional inner path; nested archives
  are further path components: `unpack:/p/outer.zip/sub/disc.iso/dir/file`. No
  query or fragment carries meaning.
- **Resolution:** kio-extras' stat-walk, with a cached image matched only at a
  `/` boundary, a cache key of device, inode, size and modification time checked
  on every request, and a plain directory redirected to `file:`. A file inside an
  image that is followed by more components, or that is listed, is opened as a
  nested archive through the same registry, up to a configurable depth (default
  in `docs/architecture.md`, section 9.1).
- **Presentation:** `kio_unpackrc` `[Nested] PresentAs`:
  - `Folder` (default): a nested archive is listed as a directory with the
    archive's MIME type and icon and the display type "Archive (browsable)";
    double-click enters it, copying gives its contents.
  - `File`: it is listed as a regular file with the archive's MIME type and icon;
    copying gives the archive file, double-click opens it with the default
    application.
  Whether an entry is a nested archive is decided from its name, so listings
  never decode entries; the content is checked when the user enters it.
  Dolphin's folder previews may still list a nested archive shown as a
  folder; spike S-6 (OQ-13) measures this, and the default `Folder` is
  provisional until then.
- **Independent of the mode:** `listDir` on a nested archive's URL lists its
  root, paths below it resolve inside it, and `get` on it returns the raw archive
  bytes.
- **Context-menu actions** inside `unpack:` (a service menu of its own with
  `X-KDE-Protocol=unpack`, separate from the local-file menu of ADR 0007; one
  selected item, supported archive MIME types), handled by the helper
  `kio-unpack-action`: "Open Archive as
  Folder" opens the entry's URL in Dolphin, "Save Archive File As…" copies the
  raw archive with `KIO::file_copy`. Both are shown in both modes, because service
  menus cannot see the worker's setting.
- Nested archives in solid blocks and the data access strategy are in ADR 0006.

## Consequences

Paths are stable and readable, work in every KIO client and survive bookmarks.
Users choose between browse-first and Total-Commander-style copying, and the
other behaviour is one right-click away. An entry named like an archive but
holding something else appears as a folder and fails when entered; accepted for
listing speed. `get` on a "directory" entry returning bytes is a deliberate
exception covered by tests.

## Alternatives considered

- **Nested archives always as plain files:** faithful copies, but browsing
  would require typing URLs.
- **Detecting copy versus browse in the worker:** impossible with current KIO.
- **Query or fragment selectors and reserved raw paths:** unnecessary; plain
  paths and `get` cover the cases, and survival of queries through KDirLister is
  unverified.
- **Sniffing every entry while listing:** exact, but turns a large listing into
  thousands of partial decodes.
