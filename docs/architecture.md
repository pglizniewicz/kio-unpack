<!--
SPDX-License-Identifier: GPL-2.0-or-later
SPDX-FileCopyrightText: 2026 kio-unpack contributors
-->

# kio-unpack architecture

Status: design for v1 (read-only). Decisions referenced here are recorded in
`docs/adr/`. Observable behaviour is specified in `docs/requirements.md`.
Prior-art citations use `repo/path:line` in the upstream sources read on
2026-10-07 (KIO `69f11e2c`, Dolphin `18e29653`, kio-extras, Krusader, Ark
`142d6fb1`, KCoreAddons, libarchive `8bb3bbdc`, libzip `0c69763e`, 7-Zip
`9128b80e`, libudfread `b0bc695`, libmirage 3.3.3 (reference for ADR 0004)).
7-Zip 26.04 and bit7z 4.1.0 were evaluated and rejected (ADR 0002).

> **Current stage: M0a** (`docs/roadmap.md`). This document describes v1 as a
> whole; implement only what the current stage needs.
>
> - **Binding now:** the three contracts at the start of section 4, and
>   section 10 for the errors M0a can already hit and for the UDS rows of the
>   image root, directories, files, symlinks and owners.
> - **Read for M0a:** sections 1 to 3; 4.1 (`FileByteSource`); 4.2; 4.3 with
>   one volume per image; 4.4, the libarchive paragraph; 4.5, the `Sniff`
>   paragraph, for ISO 9660 only; 5; 6 with its M0a list; the first bullet of
>   9 (`kdemain`, no event loop); 11; 13, the rows core unit, resolver unit,
>   worker integration and smoke, with ISO 9660 fixtures from xorriso.
> - **Intent, not to be implemented in M0a:** the registry in 4.5
>   (`BackendRegistry::open`); the other engines in 4.4; sections 7 and 8;
>   the rest of 9 (`SessionCache`, idle drop, `maxInstances`); section 12
>   (`guard()`, limits, watchdog, name decoding, fuzzing), which comes in M0b
>   and M0c.
>
> Closing a stage updates this box; nothing is moved out of the document.

## 1. Purpose and scope

kio-unpack is a KIO worker for the scheme `unpack:`. It lets Dolphin and every
other KIO client browse disc images and unusual archives as folders and copy
files out of them, the way `zip:/` works for zip files. Version 1 supports
`listDir`, `stat` and `get`. It never writes into an archive, never needs
elevated privileges, and never mounts anything (no loop devices, no FUSE).

Target formats, in milestone order: ISO 9660 with Rock Ridge and Joliet, and the
archive formats libarchive reads (M0); UDF, MBR/GPT/APM partitions, El Torito boot
images and nested archives (M2); CUE/BIN, raw dumps without a descriptor,
MDS/MDF, CCD/IMG/SUB and TOC (M3, ADR 0004). HFS and HFS+, DMG, SquashFS, WIM
and virtual-disk formats are outside 1.0 (ADR 0002); volumes in those formats
are listed as not supported. The other optical formats (NRG, CDI, MDX, B6T,
C2D, CIF, DAA, ISZ, ECM, CSO, CHD) are outside 1.0 too (ADR 0004).

## 2. How a request reaches the worker

1. On double-click, Dolphin's setting "Browse compressed files as folders"
   (`BrowseThroughArchives`, default **false**,
   `dolphin/src/settings/dolphin_generalsettings.kcfg:94-97`) decides whether a
   local archive opens as a folder at all. The setting has no influence on the
   context-menu action "Open as Folder (kio-unpack)" (ADR 0007), on typed
   `unpack:` URLs, or on archives nested inside an `unpack:` folder.
   With the setting on, when the user activates a **local** file whose MIME type is
   already known, Dolphin asks `KProtocolManager::protocolForArchiveMimetype()`
   for a protocol and, if one is returned, replaces only the URL scheme
   (`dolphin/src/views/dolphinview.cpp:1852-1878`). The lookup is an exact
   string match on the canonical MIME name, without inheritance or aliases.
   So `/home/u/disc.iso` becomes `unpack:/home/u/disc.iso`.
2. KIO finds the worker through `KPluginMetaData::findPlugins("kf6/kio")`
   (`kio/src/core/kprotocolinfofactory.cpp:91-113`) and starts the `kioworker`
   executable with the plugin's absolute path (`kio/src/core/worker.cpp:504-531`).
   Every worker except `file` and `admin` runs in its own process
   (`worker.cpp:469-477`).
3. `kioworker` resolves `kdemain` in our plugin (`kio/src/kioworker/kioworker.cpp:56-70`);
   `kdemain` constructs `UnpackWorker` and enters the blocking dispatch loop.
4. Entries inside an `unpack:` listing are not local files, so Dolphin never
   applies its archive rewrite to them. A nested archive is browsable only
   because the worker itself lists it as a directory (section 7).

Only images and archives on the local file system can be opened: the path of
an `unpack:` URL is an absolute local path (ADR 0005). An image on a remote
location such as `sftp:` or `smb:` has to be copied to a local folder first
(REQ-005).

## 3. Components

```
 KIO client (Dolphin, kioclient, file dialogs)
        │  unpack:/abs/path/disc.iso/inner/path
        ▼
 ┌───────────────────────── kio_unpack.so (src/worker) ─────────────────────────┐
 │ kdemain: rlimits, watchdog thread, backend path discovery                     │
 │ UnpackWorker : KIO::WorkerBase  ─ listDir / stat / get, guard(), UDS mapping   │
 └──────────────────────────────────────┬───────────────────────────────────────┘
                                        ▼
 ┌────────────────── libkio-unpack-core (src/core, Qt Core + KI18n) ────────────┐
 │ PathResolver ─ stat-walk on disk ▸ SessionCache ▸ inner walk ▸ nested chain   │
 │ SessionCache ─ opened images keyed by (dev, ino, size, mtime)                 │
 │ BackendRegistry ─ Sniff ▸ priority table ▸ fallback chain                     │
 │ EntryTable ─ compact immutable tree per volume                                │
 │ ByteSource ─ File | Window | Subfile | Decoding | TempFile | Track | Buffer   │
 │              | Cached (LRU block cache decorator)                             │
 └──────────────────────────────────────┬───────────────────────────────────────┘
                                        ▼
  src/backends/libarchive  (M0)   ISO 9660 RR/Joliet (M0a); zip, 7z, tar, cpio, cab, xar, lha, rar, ar (M0b)
  src/backends/udf         (M2)   libudfread: UDF on DVD/Blu-ray images
  src/backends/partition   (M2)   own MBR / GPT / APM parser, recursive open
  src/core/locate/         (M2)   ISO 9660 / tar / ZIP data locators (ADR 0006)
  src/backends/eltorito    (M2)   own boot catalog parser
  src/backends/optical     (M3)   CUE/BIN, raw dumps, MDS/MDF, CCD, TOC → tracks → recursive open
  src/backends/cli-*       (opt.) unar/lsar etc.; optional, never required by core

 src/cli/kio-unpack-cli    list | stat | cat over libkio-unpack-core (smoke, corpus, fuzz seeds)
 src/action/kio-unpack-action + 2 service menus   "Open as Folder (kio-unpack)" / "Open Archive as Folder" / "Save Archive File As…" (M2)
```

The diagram shows v1 as a whole. The components arrive stage by stage, as the
layout in `AGENTS.md` and the scopes in `docs/roadmap.md` list them: M0a has
`FileByteSource`, `Sniff`, `EntryTable`, a `PathResolver` without cache and
nesting, and the libarchive backend opened directly; `CachedByteSource` and
`SessionCache` come in M0b, `BackendRegistry` with priorities and fallback in
M2. Section 6 says which resolution steps belong to which stage.

The core library depends on Qt Core and KI18n (for user-visible strings,
ADR 0013) and has no KIO or widget dependency. Everything that can be tested,
fuzzed or driven from the command line lives there. The worker is a thin
adapter that maps URLs to resolver calls and results to `UDSEntry` and data.

## 4. Core interfaces

Three contracts are binding; changing one needs an ADR:

1. **Every input is a `ByteSource`:** read-only, seekable random access to
   bytes of known size. A read at any offset returns the requested bytes,
   short only at the end of data; a failed read raises an error that `guard()`
   maps to section 10. Files, windows into a parent, decoded streams, temporary
   files and optical tracks are all `ByteSource`s, so every backend works on
   every one of them, at any nesting level.
2. **An opened volume is an immutable entry table.** It is built once and gives
   every entry its type (directory, file, symlink, other), name and size and,
   when the format records them, modification time, mode, owner and symlink
   target; it lists the children of a directory and looks up a relative path.
   An entry's bytes are delivered in order to a sink that can stop the
   transfer; a volume may also offer a seekable view of an entry (section 7.2).
3. **Every failure is a KIO error** from the table in section 10, never a crash,
   an escaping exception or an empty listing.

The rest of this section shows one possible shape. Class and method names,
signatures, field types, memory layout and the set of concrete sources are
implementation choices, not contracts: implement the semantics above instead
of copying the sketches. Block sizes, caps, timeouts and cache sizes that the
worker chooses itself are starting values to be measured, collected in section
9.1; numbers that come from a format, from KIO or from a tool stay where they
are explained.

### 4.1 ByteSource

```cpp
// Illustrative shape, not a signature to copy.
class ByteSource {
public:
    virtual ~ByteSource() = default;
    virtual uint64_t size() const = 0;
    // Short only at end of data; failure raises an I/O error.
    virtual size_t readAt(uint64_t offset, void *buffer, size_t length) = 0;
    virtual QString describe() const = 0; // for logs and error strings
};
```

Kinds of source the design needs, named as the rest of this document refers to
them:

- `FileByteSource`: a file opened read-only; remembers the identity used to
  validate sessions (section 9).
- `WindowByteSource`: a byte range of a parent (partitions, stored entries,
  ISO 9660 extents, El Torito images).
- `SubfileByteSource`: a seekable view of one entry that a format library
  offers (libudfread files).
- `DecodingByteSource`: a forward decoder with seek emulation (section 7.2).
- `TempFileByteSource`: an entry extracted to an anonymous temporary file; last
  resort.
- `TrackByteSource`: the user data of one optical-image track.
- `BufferByteSource`: a memory span, for tests and fuzz harnesses.
- `CachedByteSource`: a caching decorator, so that every layer (libudfread's
  block reads, libarchive's directory record reads, per-sector track access,
  nested chains) gets caching without each implementation reinventing it.
- `SparseByteSource`: test-only, built with the tests and never linked into
  the worker; a large logical size with a few real byte ranges and a
  deterministic pattern elsewhere (section 13).

### 4.2 Entries and trees

The `EntryTable` holds a volume's entries in a compact form, fixed-size records
plus one string arena, so that an image with a million entries costs tens of MiB,
not hundreds. Children of a directory are contiguous and sorted by name bytes.
Besides the fields of contract 2 an entry carries flags (hidden, encrypted,
name looks like an archive) and an opaque backend handle (item index, header
ordinal). Formats without permissions get synthesised directory and file modes.
Lookup is exact, or case-insensitive when the volume allows it and the match is
unique. Build caps apply (section 9.1).

### 4.3 Volumes, providers, backends

- An `ArchiveBackend` has an id (`libarchive`, `udf`, `partition`, …), a
  priority per format, a cheap `probe` that returns a confidence and a reason,
  and an `open` that turns a `ByteSource` into an opened image.
- An opened image lists its volumes. Each volume has an id, a label (its folder
  name under `[Volumes]`), a MIME type, whether it is supported and whether its
  names are case-insensitive (Joliet, HFS, FAT, NTFS). The first volume is the
  default one (section 8); volumes are opened lazily. In this model each
  name flavour of ISO 9660 is a separate volume, opened with other libarchive
  options, although section 8 describes the flavours as one file system.
  Not every volume is a file system: the El Torito volume is a flat root whose
  entries are the boot images, each a regular file served from a
  `WindowByteSource` (REQ-066), and an audio track of a mixed-mode disc is a
  file directly under `[Volumes]` rather than a volume (REQ-071).
- A `TreeProvider` is one opened volume, used from one thread. It returns the
  volume's `EntryTable`, pushes an entry's bytes to a sink (`extract`), and
  may answer three optional questions for section 7.2: a seekable view of the
  entry without extracting it (`openSubfile()`), the byte range where the entry
  sits unmodified in the parent (`storedRange()`), and the compression method
  when the entry is one decodable stream (`streamCodec()`).

### 4.4 Engine mappings

**libarchive (M0).** The archive is read through
`archive_read_set_{open,read,seek,skip,close}_callback` and `archive_read_open1`
over a `ByteSource` (`libarchive/archive.h:528-556`), so the same backend works on
files, windows and decoded streams. One pass of `archive_read_next_header` builds
the `EntryTable`; the handle is the header ordinal. `extract()` reopens the
archive, walks headers with `archive_read_data_skip` (cheap with a seek callback)
until the ordinal matches, and streams `archive_read_data_block` into the sink.
The ISO 9660 reader uses Rock Ridge when the primary volume carries it and
switches to the Joliet tree only when it does not (`choose_volume()`,
`libarchive/archive_read_support_format_iso9660.c:1187-1197`).
libarchive-formats(5) says the opposite (`libarchive-formats.5:300-301`) and is
out of date: on 2026-10-07 libarchive 3.8.7 (Fedora 44) listed an
`xorriso -as mkisofs -R -J` image with Rock Ridge names, modes, owners and
symlinks under default options. The default open therefore needs no option and
gives REQ-024 and REQ-025 in M0a: Rock Ridge, otherwise Joliet, otherwise plain
ISO 9660. The other flavours (M2, section 8) come from format options:
`archive_read_set_format_option(a, "iso9660", "rockridge", NULL)`, the
`!rockridge` form of archive_read_set_options(3), gives the Joliet names, and
adding `!joliet` gives plain ISO 9660 names. Checked on 2026-10-07 with
libarchive 3.8.7 and a probe program on an `xorriso -as mkisofs -R -J`
image (`docs/prototypes/m0-iso9660-options.md`): `iso9660:!rockridge` listed the long Joliet names, and the symlink
was absent from the Joliet tree; `iso9660:!rockridge,iso9660:!joliet`
listed upper-case 8.3 names (`DIR_WITH/A_LONG_F.TXT`) and the symlink as a
regular file `LINK`. `archive_format_name()` was `ISO9660` in both cases, so
the backend knows the flavour from the options it set, not from libarchive. Without Rock Ridge, libarchive
reports the constants 0700 for directories and 0400 for files (`:1956-1958`)
and owner 0. The backend learns which case it got from `archive_format()`,
which is `ARCHIVE_FORMAT_ISO9660_ROCKRIDGE` only when Rock Ridge was used
(`:1222-1225`), and treats any other ISO 9660 volume as one without permissions
and synthesises modes (section 4.2); the Joliet tree has no symlinks. Subset
extraction is a linear header walk, the
same pattern Ark uses (`ark/plugins/libarchive/libarchiveplugin.cpp:248-559`).
Each `get` therefore costs a walk over every header before its entry, so
copying M files out of an image with N entries parses on the order of M × N
headers; the walk reads no file data and its directory sectors usually come
from `CachedByteSource`, but copying thousands of files from a large ISO is
noticeably slower in M0 than with the data locators of M2. The walk polls
`wasKilled()` and renews the watchdog's stall deadline after every header, so a
long walk before the first data byte is neither uncancellable nor taken for a
hang.

**libudfread (M2).** UDF images are read with libudfread through
`udfread_open_input()` and a `udfread_block_input` whose `read` callback serves
2048-byte blocks from a `ByteSource` (`libudfread/src/udfread.h:147`,
`src/blockinput.h:44-75`), so UDF inside other containers works too. Directories
come from `udfread_opendir`/`udfread_readdir`; files from `udfread_file_open`,
`udfread_file_size`, `udfread_file_seek` and `udfread_file_read`, which give true
random access and back `openSubfile()`. The upstream API reports only a type
and a MUTF-8 name per directory entry (`src/udfread.h:208-211`); we patch the
library to return times, permissions, owner, group, link count and symlink
targets, build it ourselves in both builds from pinned commit `b0bc695`, and
offer the patch upstream (ADR 0003). Fragmented UDF 2.60 metadata partitions
remain unsupported.

**Data locators (M2, ADR 0006).** libarchive does not report where an entry's
data lies, so three small parsers of our own map entries to byte ranges: an ISO
9660 directory walker (path to extents), a tar header walker (ustar, pax, GNU) and
a ZIP central-directory reader (offsets, method, sizes, ZIP64). They locate data
only; libarchive still builds the tree. A stored entry becomes a
`WindowByteSource`; a deflated ZIP entry becomes a `DecodingByteSource` over the
window of its compressed bytes.

**Partitions and El Torito (M2).** MBR, GPT and APM are parsed by our own code;
every recognised partition becomes a `WindowByteSource` and is opened
recursively through the registry. El Torito boot images come from our own
boot-catalog parser as `WindowByteSource`s.

**Optical images (M3, ADR 0004).** Our own layer reads CUE/BIN, raw dumps
without a descriptor, MDS/MDF, CCD/IMG/SUB and TOC/BIN. A sector-geometry model
describes each track: sector size (2048, 2056, 2324, 2332, 2336, 2340, 2352,
2448), where the 2048 or 2324 bytes of user data sit, subchannel presence, and
the sector type (Mode 1, Mode 2 Form 1, Mode 2 Form 2, audio). For raw dumps the
sector size comes from the CD sync pattern and the position of `CD001`/`BEA01`.
Companion files (BIN, MDF, IMG, SUB) are looked up next to the descriptor in the
same container directory, exactly first and then case-insensitively, so pairs
inside archives need no temporary files. Names stored with a path are split on
`\` and `/`; a relative path without `..` is tried below the descriptor's
directory and then reduced to its last component, and absolute, drive-letter,
UNC and `..` paths are reduced to the last component at once, so a descriptor
never opens a file outside its own directory tree (ADR 0004, REQ-097). Each data track becomes a
`TrackByteSource` and is opened through the registry; audio tracks become WAV
files (REQ-071), listed at the image root on audio-only discs and under
`[Volumes]` on mixed-mode discs. Code adapted from libmirage (GPL-2.0-or-later) keeps its
notices. libmirage itself is deferred beyond 1.0.

### 4.5 Sniffing and the registry

`Sniff` reads a few fixed regions once: the first 64 KiB, `0x8000-0x9800`
(ISO 9660 and UDF volume descriptors) and `0x400-0x600` (HFS/HFS+ headers). It
records the signatures found: `CD001`, `BEA01`/`NSR02`/`NSR03`, MBR `0x55AA`,
GPT `EFI PART`, APM `ER`/`PM`, gzip/xz/bzip2/zstd magic, the magic of the archive
formats libarchive reads, the Joliet escape sequences in supplementary volume
descriptors, and the SUSP `SP` record in the root directory record. Signatures of
formats without a reader in 1.0 (`BD`/`H+`/`HX`, DMG `koly`, `hsqs`, `MSWIM`,
`conectix`, `KDMV`, `QFI`) are recognised only to report the volume or file as not
supported. The file
extension is a tie-breaker only, except for descriptor files (`.cue`, `.ccd`,
`.mds`, `.toc`) that carry no magic.

`BackendRegistry::open(source)`:

1. Run `Sniff`. ISO 9660 and UDF descriptors outrank MBR/GPT signatures, so an
   isohybrid image is treated as a disc image (REQ-040). Its partition table
   is still read, but a partition is listed only when its range neither
   contains the ISO 9660 volume descriptors nor equals an El Torito boot
   image's range; the usual isohybrid partitions are the ISO itself or its EFI
   boot image (written from memory, UNVERIFIED; checked against distribution
   ISOs in the M2 corpus run). `CD001` together with an `Apple_HFS` entry in
   an Apple partition map, or with an HFS or HFS+ signature at 0x400, marks a
   hybrid; both lead to one HFS volume per start offset, listed as unsupported
   (REQ-065).
2. For each candidate format by descending confidence, try each backend with
   `priority(format) >= 0` by descending priority. A failed `open()` is logged
   with its reason and the chain continues.
3. When every attempt fails, raise `OpenError::Unsupported` with the collected
   reasons; the worker reports `ERR_WORKER_DEFINED` (section 10).

Priorities live in one static table, overridable in `kio_unpackrc`
(`[Backends]`). The order of equal priorities is fixed by backend id, so results
never depend on plugin discovery or hash order.

## 5. URL layout

```
unpack:/abs/path/disc.iso                          image root
unpack:/abs/path/disc.iso/dir/file.txt             entry in the default volume
unpack:/abs/path/disc.iso/[Volumes]/HFS+/Read Me   entry in an alternate volume
unpack:/abs/path/outer.zip/sub/disc.iso/dir/file   nested archive
```

The path is a plain absolute filesystem path followed by the inner path. No
query or fragment selects anything; whether those survive KDirLister and
Dolphin's item URL construction is unverified, and plain paths always do.

## 6. Path resolution

The steps below describe the v1 resolver. They are built in stages:

- **M0a** (REQ-008, REQ-009, REQ-011, REQ-026): in step 1, collapsing `//` and
  dropping a trailing `/`; in step 3, the stat-walk that selects the shortest
  prefix that is not a directory; in step 4, `FileByteSource` opened directly
  by the libarchive backend, with no cache and no registry; in step 5,
  sub-steps 2 to 4 with exact lookup only, a directory descending and a
  symlink never followed. Every failure still becomes a KIO error.
- **M0b**: step 2 (`SessionCache`, REQ-016, REQ-017); `CachedByteSource` in
  step 4; the redirects to `file:` and `ERR_MALFORMED_URL` in step 1, and the
  error mapping and the redirect of step 3 (REQ-010, REQ-012 to REQ-015);
  `ERR_DOES_NOT_EXIST` and `ERR_IS_FILE` in step 5 (REQ-018, REQ-019); `get`
  on a symlink (REQ-032, REQ-033).
- **M2**: `BackendRegistry` in step 4; `[Volumes]` in sub-step 5.1;
  case-insensitive lookup in sub-step 5.2 (REQ-020); opening a file as a
  nested archive in sub-step 5.4, and with it the chain of nested images in
  step 6 (section 7).

`PathResolver::resolve(url, operation)`, where operation is List, Stat or Get:

1. Normalise the path: collapse `//`, drop a trailing `/`. An empty path or `/`
   redirects to `file:///`. A path containing NUL or control characters is
   rejected with `ERR_MALFORMED_URL`.
2. Session lookup: a cached session with archive path `A` matches only when
   `path == A` or `path` starts with `A + "/"`. This fixes kio-extras' prefix
   test, which has no separator boundary and matches `/a/x.zip` against
   `/a/x.zipper/...` (`kio-extras/archive/kio_archivebase.cpp:58-69`). On a match,
   `stat(A)` again; when device, inode, size or mtime differ, drop the session.
3. Stat-walk on disk, as kio-extras does (`kio_archivebase.cpp:84-131`): test
   each prefix from the shortest. `ENOENT`/`ENOTDIR` gives
   `ERR_DOES_NOT_EXIST`; `EACCES`/`EPERM` gives `ERR_ACCESS_DENIED`. The code
   follows `errno` only: a path that a Flatpak sandbox does not expose is
   usually missing inside it and gives `ERR_DOES_NOT_EXIST` (REQ-013); a
   directory continues the walk;
   the first regular file is the archive. If the whole path is a directory,
   redirect to `file:`. A FIFO or device gives `ERR_CANNOT_OPEN_FOR_READING`.
   Symlinks on disk are followed, as in kio-extras.
4. Open the session: `FileByteSource` → `CachedByteSource` →
   `BackendRegistry::open`.
5. Inner walk over the remaining components, starting at the default volume's
   root:
   1. At an image root (not at the root of a volume reached through
      `[Volumes]`), the reserved component `[Volumes]` (or its collision
      variant, section 8) enters the virtual folder; the next component selects
      a volume by label.
   2. Look the component up with an exact match. If none matches and the
      volume is case-insensitive, accept a unique case-insensitive match.
      Otherwise `ERR_DOES_NOT_EXIST`.
   3. A symlink is never followed during the walk (see below).
   4. A directory descends. A file followed by more components, or a file that
      is the last component of a List operation, is opened as a nested archive
      (section 7). If it is not an archive: `ERR_IS_FILE` when components follow,
      `ERR_WORKER_DEFINED` for List.
6. The result is the session, the chain of nested images, the volume, and the
   entry or virtual folder.

Symlinks inside an image are listed as symlinks with their target. `stat()`
reports the link itself. `get()` on a symlink resolves the target lexically
inside the same volume (at most 40 hops) and redirects to it, like kio-extras
(`kio_archivebase.cpp:443-450`). A target that escapes the volume, dangles or
loops gives `ERR_DOES_NOT_EXIST`.

## 7. Nested archives

### 7.1 Presentation

A file inside an image whose name maps to a supported archive type is a
*nested archive*. The decision is name-based so that listing stays proportional
to the number of entries; the content is sniffed only when the user enters it.
If sniffing fails, entering it reports `ERR_WORKER_DEFINED`.

KIO cannot tell a copy from browsing (both send the same source-side `stat`,
ADR 0005), so the presentation is a setting, `kio_unpackrc` `[Nested] PresentAs`:

| | `Folder` (default) | `File` |
|---|---|---|
| Listing entry and `stat` | `S_IFDIR`, `UDS_DISPLAY_TYPE = "Archive (browsable)"` | `S_IFREG` |
| Both modes | `UDS_MIME_TYPE` and `UDS_ICON_NAME` of the archive type, `UDS_SIZE` of the archive | same |
| Double-click in Dolphin | enters the archive | opens it with the default application |
| Copy / drag out | extracted contents | the archive file |
| Context-menu alternative | "Save Archive File As…" | "Open Archive as Folder" |

Independent of the mode: `listDir` on the nested archive's URL lists its root,
paths below it resolve inside it, and `get` on its URL returns the archive's raw
bytes. Dolphin browses any URL whose listing succeeds and only falls back to
"is a file" handling on `ERR_IS_FILE`
(`dolphin/src/kitemviews/kfileitemmodel.cpp:3355-3357`).

A nested archive inside a 7z or RAR parent is an exception (ADR 0006): its
bytes may lie in a solid block, and libarchive does not say whether they do,
so every 7z and RAR entry is treated as solid. It is presented as a file by
default (`[Nested] PresentSolidAs`), and browsing into it asks for confirmation with a
`WarningContinueCancel` message box when the data to decode exceeds
`[Nested] ConfirmCostlyAboveMiB` (section 9.1), then extracts it once into a
temporary file for the session.

kio-unpack installs two service menus (M2), two `.desktop` files in
`${KDE_INSTALL_DATADIR}/kio/servicemenus/` (found through
`QStandardPaths::GenericDataLocation`, `kio/src/widgets/kfileitemactions.cpp:941-943`).
They must be separate files because KIO compares `X-KDE-Protocol` with the
item's scheme exactly and shows a menu that has no such key for every
protocol except `trash` (`kfileitemactions.cpp:614-639`). Both MIME lists are
generated from the same table as `archiveMimetype`, and both require one
selected item.

- **Local files** (`X-KDE-Protocol=file`, REQ-007, ADR 0007). Types the worker
  claims through `archiveMimetype` (ISO, UDF images) open on double-click.
  Every type it can open at top level, including those kio-extras claims,
  `.cue` sheets and their derived types such as `.docx` or `.epub`, also gets
  the action "Open as Folder (kio-unpack)", which opens the file's `unpack:`
  URL. A `.cue` inside an archive is an ordinary nested archive.
- **Items inside `unpack:`** (`X-KDE-Protocol=unpack`, REQ-054, ADR 0005), for
  the supported archive MIME types: "Open Archive as Folder" and "Save Archive
  File As…". Both actions are always shown, because service menus cannot see
  the worker's setting.

Both menus run the helper `kio-unpack-action` (`src/action/`,
Qt Widgets + KIO): `open-as-folder <url>` starts `dolphin <url>`, which hands the
URL to the running instance; `save-archive <url>` asks for a destination and runs
`KIO::file_copy`, which reads the source with `get` and never lists it.

### 7.2 Never unpack more than necessary

A nested archive, or a file inside it, is served without extracting the outer
archive and without decoding more of it than the position of the requested data
requires. Each nesting step takes the first applicable rung:

| Rung | Mechanism | Applies to | Cost |
|---|---|---|---|
| 1 | `TreeProvider::openSubfile()` | parents whose reader offers random access to an entry: UDF (libudfread), optical-image tracks (M3) | zero copy, true random access |
| 2 | `WindowByteSource` over `storedRange()` | entries stored without compression whose data offset a locator knows: ISO 9660 extents, tar entries, ZIP stored entries, partitions, El Torito images | zero copy, true random access |
| 3 | `DecodingByteSource` over `streamCodec()` | deflated ZIP entries and gzip (zlib, with checkpoints), xz (liblzma, block index when multi-block), bzip2 and zstd (forward only) | decodes only as far as reads go; backward seeks restart at the nearest checkpoint |
| 4 | `TempFileByteSource` | 7z/RAR entries (treated as solid, ADR 0006), or when rung 3's backward-seek budget is exhausted | full extraction of the one entry, under size caps |

`DecodingByteSource` decodes forward on demand and keeps an LRU block cache.
For deflate and gzip it records checkpoints (decoder window and bit offset, the
method of zlib's `zran.c` example) so a backward seek outside the cache restarts
at the nearest checkpoint; for multi-block xz it seeks through liblzma's block
index; bzip2 and zstd restart at the beginning. ISO 9660 keeps its
volume descriptors and directory records near the start of the image, so listing
`disc.iso.gz` or a deflated `disc.iso` inside a zip decodes only the first part
of the stream; copying one file out decodes up to that file's extent.

Every rung counts what it does in an `UnpackStats` structure (bytes decoded,
restarts, temp bytes written). Tests use it to prove that rungs 1-3 never create
a temp file. 7z and RAR parents cannot meet the principle; ADR 0006 sets the
policy for them. Budgets for rung 3 are OQ-5.

Temp files (rung 4) live in `QStandardPaths::CacheLocation` + `/kio-unpack`
(on disk; `/tmp` is RAM-backed tmpfs on Fedora; under Flatpak this is
`~/.var/app/<id>/cache`), are opened with `O_TMPFILE` so nothing is left behind
after a crash, and obey per-file and per-session caps. On a file system
without `O_TMPFILE` (`EOPNOTSUPP`, for example NFS, UNVERIFIED; `EISDIR` on
a kernel without it) the file is created with `mkostemp()` and unlinked at once, so
only a crash between those two calls leaves it behind (ADR 0006, REQ-053).

The nesting depth is limited (section 9.1). Opened nested images are kept in a
small per-session LRU keyed by inner path.

## 8. Volumes and hybrid images

An image may contain more than one file system: ISO 9660 plus HFS or HFS+ on
hybrid Mac/PC discs, ISO 9660 plus UDF on DVDs, El Torito boot images,
additional sessions or tracks, partitions. Rock Ridge and Joliet are *name
flavours* of one ISO 9660 volume, not separate volumes.

- The root of `unpack:/…/disc.iso` shows the **default volume**, chosen in this
  order: UDF (when libudfread opens it), ISO 9660 with Rock Ridge, ISO 9660 with
  Joliet, plain ISO 9660 (ADR 0008).
- When an image has any alternate volume or name flavour, the root also contains
  one reserved virtual folder `[Volumes]`. Its children are the volumes by label:
  `ISO9660`, `ISO9660 (Joliet)`, `ISO9660 (Rock Ridge)`, `UDF`, `HFS+`, `HFS`,
  `El Torito`, `Session 2`, `Track 02.wav` on mixed-mode discs, … The default volume is
  listed there too.
- Provisional until the prototype of spike S-4 (OQ-11, ADR 0008): `[Volumes]` is
  reported as `S_IFDIR` with `UDS_LINK_DEST`. KIO's recursive
  listing does not descend into links and `CopyJob` copies such an entry from
  `unpack:` to another protocol with `get`, so copying the image root copies
  the default volume once and writes one small text file `[Volumes]` that names
  the volumes (ADR 0008, REQ-092, REQ-093). A plain directory would be copied
  once per volume and name flavour. Whether Dolphin and `CopyJob` really behave
  this way is checked by spike S-4 at the start of M0b (`docs/roadmap.md`); if not, ADR 0008's
  fallback applies.
- If the default volume itself contains an entry named `[Volumes]`, the real
  entry wins and the virtual folder becomes `[Volumes~1]` (then `~2`, …).
- Partitions of an isohybrid image and the HFS side of a hybrid Mac/PC disc
  follow section 4.5, step 1: a partition that is the ISO itself or an El
  Torito boot image is not listed again, and an HFS volume found both through
  the Apple partition map and through the signature at 0x400 is listed once.
- `El Torito` holds one regular file per boot image; their names are OQ-12.
- HFS and HFS+ volumes (the Mac side of hybrid discs) have no reader in 1.0
  (ADR 0002; HFS readers are later ideas in `docs/roadmap.md`). They are listed under `[Volumes]` with display
  type "HFS (not supported)" or "HFS+ (not supported)", and entering one reports
  `ERR_WORKER_DEFINED`.
- Krusader exposes every descriptor as a top-level folder
  (`krusader/plugins/iso/kiso.cpp:464-503`) and selects sessions through a URL
  fragment (`krusader/plugins/iso/iso.cpp:208`); we keep ordinary paths stable
  and avoid fragments instead.

## 9. Worker process, sessions and limits

- `kdemain` constructs a `QCoreApplication`, as every KIO worker does
  (`kio/src/kioworkers/file/file.cpp:74`); KI18n, `QStandardPaths`,
  `QMimeDatabase` and KConfig need it. `exec()` is never called: requests are
  served by KIO's blocking dispatch loop, which only processes deferred
  deletes between commands (`kio/src/core/slavebase.cpp:364`). So `QTimer`,
  queued connections and socket notifiers do not fire while a request runs;
  calls that wait for the client, such as `messageBox()`, block on the KIO
  connection and need no event loop. `UnpackWorker` is single-threaded and
  backends are treated as single-threaded. The only extra thread is the
  watchdog.
- `SessionCache` keeps a few opened images per worker process (the number is
  provisional until spike S-1, section 9.1), keyed by
  `(st_dev, st_ino, st_size, st_mtim)` and the real path. Every request re-stats
  the archive; a mismatch drops the session.
- After each operation the worker arms `setTimeoutSpecialCommand()` with the
  idle period (`kio/src/core/workerbase.h:818`); `special("idle")` drops all
  sessions, closing temp files and freeing trees. KIO reaps idle workers after
  3 minutes (`kio/src/core/scheduler.cpp:28-29`).
- Entries, names, nesting, temporary data, memory and time are capped; the caps
  are configurable in `kio_unpackrc` (REQ-083).
- `maxInstances` starts at KIO's default of 1. Raising it for parallel tabs
  depends on how the scheduler assigns host-less jobs (OQ-2); spike S-1 at the
  start of M0b decides it and the `SessionCache` size before they are built.

### 9.1 Tunable defaults

Every number below is a starting value, not a contract. Each is measured in the
milestone named in the last column and changed there without an ADR; this
table, `docs/requirements.md` and the configuration loader are updated
together. Tests must not depend on these values: they set their own, through
configuration where a key exists and otherwise when constructing the component.
"Not set" means the value is chosen by that measurement.

| Setting | `kio_unpackrc` key | Starting value | Settled by |
|---|---|---|---|
| Idle period before sessions are dropped (REQ-081); shorter than KIO's 3-minute reap | `[Limits] IdleSeconds` | 90 s | M0b, with spike S-1 |
| Open images per worker (`SessionCache`) | none | 3 | spike S-1 (M0b) |
| `CachedByteSource` block size | none | 64 KiB | M0b, listing times on the corpus |
| `CachedByteSource` memory budget per worker | none | not set | M0b, listing times on the corpus |
| Chunk size of `get()` data, which also bounds cancellation latency (REQ-077) | none | 1 MiB | M0b |
| Entries per volume (REQ-080) | `[Limits] MaxEntries` | 2,000,000 | M0c, corpus run |
| String arena per volume (REQ-080) | `[Limits] MaxArenaMiB` | 512 MiB | M0c, corpus run |
| `RLIMIT_DATA` (ADR 0011) | `[Limits] MemoryLimitMiB` | 2 GiB | M0c, peak memory on the corpus |
| Open, list and stall deadlines of the watchdog (REQ-078) | `[Limits] OpenTimeoutSec`, `ListTimeoutSec`, `StallTimeoutSec` | not set | M0c, timings on the corpus |
| Nesting depth (REQ-051) | `[Limits] MaxNestingDepth` | 4 | M2 |
| Temporary file size (REQ-052) | `[Limits] MaxTempFileMiB` | 2 GiB | M2 |
| Temporary data per session (REQ-052) | `[Limits] MaxTempTotalMiB` | 4 GiB | M2 |
| Confirmation threshold for costly decoding (REQ-086) | `[Nested] ConfirmCostlyAboveMiB` | 64 MiB | M2 (spike S-3 in M0b decides where the box appears) |
| Backward-seek budget, checkpoint spacing and cache of `DecodingByteSource` | none | not set | M2 (OQ-5) |

## 10. Errors and UDS fields

| Condition | KIO error |
|---|---|
| On-disk prefix missing (`ENOENT`, `ENOTDIR`); inner path missing; symlink dangling or escaping | `ERR_DOES_NOT_EXIST` |
| On-disk `EACCES` or `EPERM` (a path a sandbox does not expose usually gives `ENOENT`, row above) | `ERR_ACCESS_DENIED` |
| `get()` on a directory or volume root (not on a nested archive, whose `get` returns its raw bytes, nor on `[Volumes]`, whose `get` returns the volume list) | `ERR_IS_DIRECTORY` |
| Path continues below a plain file | `ERR_IS_FILE` |
| On-disk open or read failure (`EIO`, FIFO, device, `EMFILE`) | `ERR_CANNOT_OPEN_FOR_READING` |
| No backend accepts the data (unknown, corrupt, encrypted headers), `get` of an encrypted entry (REQ-101), unsupported volume, nesting too deep, entry cap exceeded | `ERR_WORKER_DEFINED` with a message |
| Corrupt or truncated data during extraction | `ERR_CANNOT_READ` |
| Temp file `ENOSPC`/`EFBIG`, temp caps exceeded | `ERR_DISK_FULL` |
| `std::bad_alloc` | `ERR_OUT_OF_MEMORY` |
| `wasKilled()` observed | `ERR_USER_CANCELED` |
| NUL or control characters in the path | `ERR_MALFORMED_URL` |
| Any write operation, `open`/`read`/`seek` | `ERR_UNSUPPORTED_ACTION` (WorkerBase default) |

`ERR_WORKER_DEFINED` matters for usability: Dolphin then offers to open the file
with the default application for the first MIME type in `archiveMimetype`
(`dolphin/src/dolphinviewcontainer.cpp:1112-1134`), which is Ark or ISO Image
Writer depending on the user's associations.

| Item | UDS fields |
|---|---|
| Image root, also `stat(unpack:/p/disc.iso)` | `S_IFDIR`; `UDS_MIME_TYPE` = the image's MIME type; `UDS_SIZE` and mtime of the file on disk; `UDS_DISPLAY_NAME` = file name |
| Directory | `S_IFDIR`; `UDS_MIME_TYPE = inode/directory`; `UDS_ACCESS` = mode & 0555 or 0555 |
| File | `S_IFREG`; `UDS_SIZE`; `UDS_ACCESS`; `UDS_MIME_TYPE` only when `stat` details ask for it (sniffed from the first 64 KiB) |
| Symlink | `S_IFLNK`; `UDS_LINK_DEST` |
| Nested archive | `S_IFDIR` with `UDS_DISPLAY_TYPE = "Archive (browsable)"` (`PresentAs=Folder`) or `S_IFREG` (`PresentAs=File`); `UDS_MIME_TYPE` and `UDS_ICON_NAME` of the archive type; `UDS_SIZE` |
| `[Volumes]` | `S_IFDIR`; `UDS_LINK_DEST` (value settled by OQ-11); `UDS_MIME_TYPE = inode/directory` although `get` returns `text/plain` (deliberate, ADR 0008); `UDS_DISPLAY_TYPE` = "Volumes (virtual folder)"; `UDS_SIZE` = length of the text from `get` (REQ-093) |
| Volume roots | `S_IFDIR`; `UDS_MIME_TYPE = inode/directory`; `UDS_DISPLAY_TYPE` = label, with " (not supported)" when applicable |
| Owner | `UDS_USER`/`UDS_GROUP` only when the format records them (Rock Ridge, tar, cpio, UDF) |

`get()` announces `totalSize`, streams fixed-size chunks (section 9.1),
reports `processedSize`, emits `mimeType()` once from the first chunk via
`QMimeDatabase::mimeTypeForFileNameAndData`, and ends with an empty `data()`
(the kio-extras pattern, `kio-extras/archive/kio_archivebase.cpp:415-523`).

## 11. Packaging and paths

- The plugin is built with `kcoreaddons_add_plugin(kio_unpack INSTALL_NAMESPACE "kf6/kio")`
  and installs to `${KDE_INSTALL_PLUGINDIR}/kf6/kio/kio_unpack.so`
  (`kcoreaddons/KF6CoreAddonsMacros.cmake:61-68`). In the build tree it lands in
  `build/bin/kf6/kio/` (ECM `KDECMakeSettings.cmake:271`), which is what the dev
  loop and the integration tests put on `QT_PLUGIN_PATH`.
- Metadata is embedded with
  `Q_PLUGIN_METADATA(IID "org.kde.kio.worker.unpack" FILE "unpack.json")`:

```json
{
    "KDE-KIO-Protocols": {
        "unpack": {
            "protocol": "unpack",
            "Class": ":local",
            "Icon": "package-x-generic",
            "input": "filesystem",
            "output": "filesystem",
            "source": true,
            "reading": true,
            "listing": ["Name", "Type", "Size", "Date", "Access", "Owner", "Group", "Link", "MimeType"],
            "determineMimetypeFromExtension": true,
            "archiveMimetype": ["application/vnd.efi.iso"],
            "config": "unpack"
        }
    }
}
```

  The `protocol` key is informational; KIO uses the object key
  (`kio/src/core/kprotocolinfofactory.cpp:97-111`). The protocol icon is the
  generic package icon that kio-extras' archive worker uses
  (`kio-extras/archive/archive.json`, `bb779b08`), not an optical-disc icon,
  because the scheme serves ZIP and tar as well; an image root shows the icon
  of its own MIME type (REQ-023). The MIME list grows per
  milestone under the rules of ADR 0007.
- Format libraries are linked normally and found by CMake (`pkg_check_modules`
  for libudfread, `find_package(LibArchive)`); no literal `/usr` appears in code.
- All user-visible strings use KI18n with the translation domain `kio6_unpack`;
  catalogues live in `po/` and are installed with `ki18n_install(po)` (ADR 0013).
- Every third-party dependency is pinned in `flatpak/modules/` and builds without
  network access in the flatpak-builder sandbox.

## 12. Robustness

- **Crash isolation.** The worker runs in its own process. A crash in a backend
  surfaces as `ERR_WORKER_DIED` ("The process for the unpack protocol died
  unexpectedly"), Dolphin keeps running, and the next request starts a fresh
  worker (`kio/src/core/worker.cpp:305-326`).
- **Exception firewall.** Every `WorkerBase` virtual runs inside `guard()`, which
  maps `OpenError`, `IoError`, `std::bad_alloc` and any
  other `std::exception` to the error table. An escaping exception would
  otherwise terminate the worker.
- **Resource limits.** `kdemain` sets `RLIMIT_DATA` (covers heap and private
  anonymous mappings; unlike `RLIMIT_AS` it does not count file mappings, thread
  stacks reserved by Qt, or shared libraries), `RLIMIT_FSIZE` at the temp cap,
  and `RLIMIT_CPU` as a last resort. Limits are skipped in sanitizer builds,
  where ASan reserves large address ranges. They back up the user-space
  counting in `UnpackStats` and the temp-file caps, which stop writing before a
  limit is reached. The kernel enforces them with signals whose default action
  is to terminate with a core dump, so `kdemain` sets `SIGXFSZ` to `SIG_IGN`
  (a write past `RLIMIT_FSIZE` then fails with `EFBIG` and becomes
  `ERR_DISK_FULL`) and handles `SIGXCPU` like the watchdog: one `write()` to
  stderr, then `_exit()`. The hard CPU limit sits a few seconds above the soft
  one as a `SIGKILL` backstop (ADR 0011).
- **Hangs.** KIO has no client-side response timeout; a hung worker blocks its
  job until the user cancels. A watchdog thread holds a deadline per operation
  (opening and listing: absolute; extraction: a stall deadline renewed by every
  sink callback, so large copies are not cut). On expiry it writes one line to
  stderr and calls `_exit()`, and KIO reports `ERR_WORKER_DIED`. It never uses
  `abort()`, because KIO initialises KCrash in every out-of-process worker
  (`kio/src/core/slavebase.cpp:255-261`) and a crash dialog would appear, and it
  never uses `alarm()`, because KIO uses `SIGALRM` for its own 5-second kill after
  `SIGTERM` (`slavebase.cpp:229-243`).
  The deadline counts only time spent in our code and the backends (ADR 0011):
  `guard()` arms it with an RAII scope and disarms it before returning to KIO's
  dispatch loop, so an idle worker has no deadline; arm, disarm and the expiry
  decision share one mutex and a generation number, and the watchdog sleeps on
  a condition variable without timeout while disarmed. Stall renewals are an
  atomic timestamp. Calls that send to or wait for the client (`data()`,
  `listEntry()`, `listEntries()`, `messageBox()`, `warning()`) pause the clock,
  because a message box waits for the user and a client that stops reading
  blocks our writes. The watchdog does not interact with the idle
  `special("idle")` timer, which KIO runs.
- **Cancellation.** Loops poll `wasKilled()`; sinks return false so extraction loops
  stop.
- **Untrusted names.** Path components that are empty, `.` or `..` are dropped.
  Archives are opened without libarchive's `hdrcharset`. The single header pass
  keeps names that are valid UTF-8, with or without the ZIP UTF-8 flag, and
  stores the raw bytes of the others, which are decoded with `iconv` after the
  pass from `[Names] LegacyCodepage` (ZIP) or `LegacyUnixCharset` (tar, cpio,
  Rock Ridge); unset keys follow the system language (REQ-028, REQ-099, ADR
  0011). `kdemain` sets `LC_CTYPE` to `C.UTF-8` if the inherited locale is not
  UTF-8, because libarchive converts flagged UTF-8 names to the locale's
  charset. Only then are undecodable bytes and control characters replaced;
  in ZIP a name with `\` and no `/` uses `\` as the separator, as libarchive
  already does for MS-DOS hosts, before components are checked (REQ-098); colliding names get a
  ` (2)` suffix; symlink targets are stored verbatim and never followed outside
  their volume. Sizes declared by the format are hints; actual bytes are counted
  against the caps.

  Default legacy charsets by system language (REQ-099), checked on 2026-10-07.
  The ZIP column is Windows' OEM code page per locale as Wine records it
  (`wine/tools/make_unicode`, key `oemcp`, Wine `59416cf5`; e.g. `pl` at line
  1257, `en-GB` at 701, `zh-Hant` at 1596). The Unix column is the non-UTF-8
  charset of the locale in glibc (`glibc/localedata/SUPPORTED`, glibc
  `46d325b6`; e.g. `pl_PL/ISO-8859-2` at line 379, `be_BY/CP1251` at 59,
  `et_EE/ISO-8859-1` at 211). Two cells are choices, not lookups: for `ru`
  glibc's plain locale is ISO-8859-5 (line 393), but KOI8-R (line 391) is
  kept because it was the common charset on Russian Unix systems
  (UNVERIFIED); for `zh` in China GB18030
  (line 493) is taken because it is a superset of the default GB2312 (line
  496). 7-Zip follows the OEM code page of the Windows system, not the UI
  language; `QLocale::system()` stands in for it. Languages that Windows maps
  to CP850 but that are not in the table (for example `ca`, `af`, `cy`) get
  the CP437 fallback.

  | Language | ZIP (`LegacyCodepage`) | tar, cpio, Rock Ridge (`LegacyUnixCharset`) |
  |---|---|---|
  | pl, cs, sk, hu, hr, sl, ro | CP852 | ISO-8859-2 |
  | ru | CP866 | KOI8-R |
  | be, bg | CP866 | CP1251 |
  | uk | CP866 | KOI8-U |
  | de, fr, es, it, nl, pt, da, sv, nb, fi | CP850 | ISO-8859-1 |
  | en outside the US (GB, AU, CA, IE, NZ) | CP850 | ISO-8859-1 |
  | el | CP737 | ISO-8859-7 |
  | tr | CP857 | ISO-8859-9 |
  | lt, lv | CP775 | ISO-8859-13 |
  | et | CP775 | ISO-8859-1 |
  | he | CP862 | ISO-8859-8 |
  | ja | CP932 | EUC-JP |
  | zh (China) | CP936 | GB18030 |
  | zh (Taiwan) | CP950 | BIG5 |
  | zh (Hong Kong) | CP950 | BIG5-HKSCS |
  | ko | CP949 | EUC-KR |
  | any other, including en-US | CP437 | ISO-8859-1 |
- **Fuzzing.** libFuzzer harnesses (`-DKIO_UNPACK_FUZZ=ON`, clang +
  compiler-rt, ASan + UBSan) for `Sniff`, every parser of our own (partitions,
  El Torito, data locators, the checkpointing decoder, optical descriptors),
  each backend through `BufferByteSource` (open, list, extract the first
  entries to a null sink) and the resolver; CI runs each for 60 seconds on
  every push from M0c on (REQ-082). Run limits: `-timeout=20 -rss_limit_mb=2048`. Seeds are
  generated fixtures plus small corpus samples. Unless libarchive and libudfread are
  built with sanitizers too (OQ-4), only our code is instrumented.
- **No helper process in 1.0** (ADR 0011); the worker's
  own process isolation, limits and watchdog remain.

## 13. Testing

| Level | Mechanism | Fixtures |
|---|---|---|
| Core unit | QtTest + `ecm_add_test`; backends driven through `FileByteSource`/`BufferByteSource` | generated at test time: `xorriso -as mkisofs -R -J`, `genisoimage -hfs`, El Torito with a dummy boot image, `mksquashfs`, `mkfs.vfat` + `mtools`, `zip -0`/`zip -9`, `gzip`/`xz`; checked in only when < 100 KB and not generable |
| Resolver unit | fake backend, in-memory sessions | none |
| Worker integration | `QT_PLUGIN_PATH=<build>/bin` (KIO precedent: `kio/autotests/http/CMakeLists.txt:107-114`), `QStandardPaths::setTestModeEnabled(true)`, `KIO::listDir`/`stat`/`storedGet` | generated |
| Smoke | `kioclient --platform offscreen ls/cat/stat`, `kio-unpack-cli` | generated |
| Corpus | `scripts/corpus-fetch.sh` downloads a short list of small samples from `sembiance.com/fileFormatSamples/archive/` into the untracked `.cache/`; `scripts/corpus-smoke.sh` lists and extracts and tabulates ok/error/timeout/crash | real images, never committed |
| Fuzz | section 12 | generated seeds |
| Stress | `-DKIO_UNPACK_STRESS_TESTS=ON`, ctest label `stress`; run by the periodic CI job, not by an ordinary `ctest` | real large fixtures: a 512 MiB `disc.iso.gz`, a 4.5 GiB file in a UDF image |

Ordinary tests stay small: no test writes more than 64 MiB of fixtures or
decodes more than 64 MiB. Large sizes and offsets are reached with
`SparseByteSource`, a test-only `ByteSource` with a large logical size that
serves a few real byte ranges and, everywhere else, a deterministic pattern
computed from the offset, so any read can be checked without a reference file.
Real large data belongs to stress tests.

## 14. Open questions

Tracked in `docs/requirements.md`, section "Open questions".
