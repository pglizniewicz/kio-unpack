<!--
SPDX-License-Identifier: GPL-2.0-or-later
SPDX-FileCopyrightText: 2026 kio-unpack contributors
-->

# 0007. Dolphin integration: MIME claims and the "Open as Folder" action

Date: 2026-10-07
Status: Accepted

## Context

Dolphin picks the worker for a local archive with
`KProtocolManager::protocolForArchiveMimetype()`
(`kio/src/core/kprotocolmanager.cpp:368-385`). It builds a map once per process
from every worker whose `input` is `filesystem`, iterating a `QHash`; when two
workers claim the same type, the last one inserted wins, and Qt randomises
`QHash` seeds per process (Qt documentation of QHash). KIO's own test accepts
either `zip` or Krusader's `krarc` for `application/zip`
(`kio/autotests/kprotocolinfotest.cpp:97-99`). The lookup uses the file's
canonical MIME name as an exact string, without aliases or inheritance
(`dolphin/src/views/dolphinview.cpp:1868-1876`). On shared-mime-info 2.4 the
canonical ISO type is `application/vnd.efi.iso`.

The rewrite happens only on double-click of a local file and only while
Dolphin's setting "Browse compressed files as folders" is on
(`BrowseThroughArchives`, default false, `dolphin_generalsettings.kcfg:94-97`).

kio-extras claims `application/x-archive`, `application/x-7z-compressed`,
`application/zip` and seven tar types (`kio-extras/archive/archive.json`), no
ISO. Its `filter` worker handles gzip, bzip2, xz, lzma and zstd as streams
(`kio-extras/filter/filter.json`), which the archive lookup ignores. Krusader's
iso worker claims only non-canonical ISO aliases (`krusader/plugins/iso/iso.json:6-10`).

`.cue` files (`application/x-cue`) describe disc images but also accompany ripped
music albums. Service menus can be shown for local files by MIME type and match
types with inheritance (`kio/src/widgets/kfileitemactions.cpp:62-86`); 35 types
inherit `application/zip`, among them OOXML documents, EPUB, Java archives and
comic book archives. On `ERR_WORKER_DEFINED` Dolphin offers to open the file with
the default application for the first claimed type
(`dolphin/src/dolphinviewcontainer.cpp:1112-1134`).

## Decision

- **Claims:** only canonical MIME types that no other known KIO worker claims;
  `application/vnd.efi.iso` first. Never the kio-extras types, never
  `application/x-cue`, never `application/octet-stream`. Further disc-image types
  are added under OQ-1 after checking real samples.
- **Alternative action:** one service menu for single local files
  (`X-KDE-Protocol=file`; without the key KIO would show it for every protocol
  except `trash`, `kio/src/widgets/kfileitemactions.cpp:614-639`), separate
  from the `unpack:` menu of ADR 0005, offers
  "Open as Folder (kio-unpack)" (Polish: "Otwórz jako folder (kio-unpack)") for
  every MIME type the worker can open at top level: the types it claims, the
  kio-extras types, the other libarchive formats, single-stream compression,
  `application/x-cue` and the disc-image descriptor types. Derived types are not
  excluded, so the action also appears on `.docx`, `.epub`, `.jar` and `.cbz`, as
  Total Commander plugins do. The list is generated at build time from the same
  table as `archiveMimetype`. The action runs `kio-unpack-action open-as-folder`
  on the file's `unpack:` URL and does not depend on the browse setting.
- **CUE files:** a local `.cue` keeps its default application on double-click and
  is browsed only through the action; a `.cue` inside an archive is a nested
  archive (ADR 0005).
- **Documentation** states that the browse setting affects only double-click on
  local files; the action, typed `unpack:` URLs, other KIO clients and nested
  archives work regardless.
- Later, propose upstream a deterministic tie-break for
  `protocolForArchiveMimetype()` (for example a priority key, as Ark does with
  `X-KDE-Priority`, `ark/kerfuffle/plugin.cpp:21-25`).

## Consequences

ISO files open in kio-unpack on double-click deterministically, kio-extras keeps
ZIP, 7z, tar and ar, music players keep `.cue` albums, and every supported format
is one right-click away. If another worker later claims the same canonical type,
the choice becomes random again; the overlap test covers only the claims known
today. ZIP-based documents show one more context-menu entry.

## Alternatives considered

- **Claiming the kio-extras types:** makes Dolphin's choice random per launch.
- **Claiming MIME aliases such as `application/x-cd-image`:** never match
  Dolphin's canonical lookup and only add collisions.
- **Claiming `application/x-cue`:** breaks the default action for ripped albums.
- **Excluding ZIP-based types from the action:** fewer menu entries, but no way
  to look inside documents and packages; rejected by the owner.
- **Patching Dolphin to prefer kio-unpack:** the fix belongs in KIO.
