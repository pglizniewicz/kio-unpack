<!--
SPDX-License-Identifier: GPL-2.0-or-later
SPDX-FileCopyrightText: 2026 kio-unpack contributors
-->

# kio-unpack requirements

Externally observable behaviour of the `unpack:` worker, written in EARS
notation. Patterns used:

- **Ubiquitous**: The `<system>` shall `<response>`.
- **Event-driven**: When `<trigger>`, the `<system>` shall `<response>`.
- **State-driven**: While `<state>`, the `<system>` shall `<response>`.
- **Unwanted behaviour**: If `<condition>`, then the `<system>` shall `<response>`.
- **Optional feature**: Where `<feature>`, the `<system>` shall `<response>`.

Each requirement has an ID, the milestone that delivers it (M0 is split into
the stages M0a, M0b and M0c, `docs/roadmap.md`), and how it is verified. "Stress" means a test registered only with
`-DKIO_UNPACK_STRESS_TESTS=ON` and run by the periodic CI job, never by an
ordinary `ctest`. "Unit" means a QtTest on `libkio-unpack-core`; "integration" means a
QtTest that drives the built worker through KIO jobs with
`QT_PLUGIN_PATH=<build>/bin`; "smoke" means `kioclient --platform offscreen`;
"manual" means a scripted check in Dolphin. Tests name the requirement they
cover (`// REQ-NNN`).

Only behaviour decided in the planning session is specified. Undecided points
are listed under *Open questions* and must not be implemented by guessing.

"The worker" means the `unpack` KIO worker process. "Image" means the archive or
disc image file on disk that a URL points into. Fixture names refer to files the
test suite generates.

## 1. Registration and Dolphin integration

**REQ-001** (M0a, ubiquitous) The worker shall register the protocol `unpack`
as a plugin in the `kf6/kio` namespace with `input` and `output` of type
`filesystem`, `Class` `:local`, and `reading` enabled.
*Verified by:* integration: `KProtocolInfo::isKnownProtocol("unpack")`,
`KProtocolInfo::protocolClass("unpack") == ":local"`,
`KProtocolManager::supportsReading`.

**REQ-002** (M0a, ubiquitous) The worker shall declare
`application/vnd.efi.iso` as the first entry of its `archiveMimetype` list.
*Verified by:* integration: `KProtocolInfo::archiveMimetypes("unpack").first()`;
`KProtocolManager::protocolForArchiveMimetype("application/vnd.efi.iso") == "unpack"`.

**REQ-003** (M0a, ubiquitous) The worker shall declare in `archiveMimetype` only
canonical MIME type names, that is, names `n` for which
`QMimeDatabase().mimeTypeForName(n).name() == n`.
*Verified by:* integration over the embedded JSON:
`KProtocolInfo::archiveMimetypes("unpack")`, which KIO reads from the
metadata of the built plugin.

**REQ-004** (M0a, ubiquitous) The worker shall not declare in `archiveMimetype`
any MIME type declared by the kio-extras archive worker
(`application/x-archive`, `application/x-7z-compressed`, `application/zip`,
`application/x-tar`, `application/x-compressed-tar`,
`application/x-bzip-compressed-tar`, `application/x-webarchive`,
`application/x-lzma-compressed-tar`, `application/x-xz-compressed-tar`,
`application/x-zstd-compressed-tar`).
*Verified by:* integration over the embedded JSON against this list, as for
REQ-003.

**REQ-005** (M0a, ubiquitous) The project documentation shall state that
Dolphin's setting "Browse compressed files as folders" (disabled by default)
affects only one thing: whether a double-click on a local image or archive
opens it as a folder. It shall also state that everything else works regardless
of that setting: the context-menu action "Open as Folder (kio-unpack)", typed
`unpack:` URLs, other KIO clients, and archives or images nested inside an
`unpack:` folder. It shall further state that only images and archives on the
local file system can be opened (`unpack:` takes an absolute local path,
ADR 0005); one on a remote location such as `sftp:` or `smb:` has to be
copied to a local folder first.
*Verified by:* review of `README.md`, `docs/architecture.md` section 2 and
`flatpak/README.md` (manual).

**REQ-006** (M0a, event-driven) When Dolphin, with "Browse compressed files as
folders" enabled, activates a local file of type `application/vnd.efi.iso`, the
worker shall receive the request and the view shall show the image's root
directory.
*Verified by:* manual: dev loop (ADR 0009) with fixture `rr.iso`.

**REQ-007** (M2, ubiquitous) kio-unpack shall install a Dolphin service menu for
single local files (`X-KDE-Protocol=file`), separate from the menu of REQ-054,
offering "Open as Folder (kio-unpack)" (Polish: "Otwórz jako
folder (kio-unpack)") for every MIME type the worker can open at top level,
including the types kio-extras claims and their derived types, without
`ExcludeServiceTypes`; its MIME list shall be generated from the same table as
`archiveMimetype`.
*Verified by:* unit: the installed `.desktop` file has `X-KDE-Protocol=file`
and `X-KDE-RequiredNumberOfUrls=1`, its `MimeType` equals the generated list,
contains `application/zip`, `application/x-7z-compressed`,
`application/x-tar`, `application/vnd.rar` and `application/gzip`, has no
`ExcludeServiceTypes`, and has a Polish name; manual: the action appears on a
`.zip` and a `.docx` in Dolphin and opens them under `unpack:`.

## 2. URL resolution

**REQ-008** (M0a, ubiquitous) The worker shall interpret the path of an
`unpack:` URL as an absolute local path to an image, followed optionally by a
path inside the image.
*Verified by:* integration: `listDir`/`stat`/`get` on `rr.iso`, `rr.iso/dir`,
`rr.iso/dir/file.txt`.

**REQ-009** (M0a, event-driven) When the worker resolves a URL, it shall select
as the image the shortest prefix of the path that is not a directory on disk.
*Verified by:* unit (resolver) with nested on-disk directories and an image
whose inner path repeats on-disk names.

**REQ-010** (M0b, event-driven) When the whole path of an `unpack:` URL is an
existing directory on disk, the worker shall redirect to the equivalent `file:`
URL.
*Verified by:* integration: `KIO::listDir(unpack:<tmpdir>)` reports a
redirection to `file:<tmpdir>`.

**REQ-011** (M0a, ubiquitous) The worker shall treat a URL with and without a
trailing slash as the same location.
*Verified by:* integration: `listDir` on `rr.iso` and `rr.iso/` yield the same
entries.

**REQ-012** (M0b, unwanted) If a prefix of the path does not exist on disk, then
the worker shall fail with `ERR_DOES_NOT_EXIST`.
*Verified by:* integration.

**REQ-013** (M0b, unwanted) If the system denies access to a prefix of the path
(`EACCES` or `EPERM`), then the worker shall fail with `ERR_ACCESS_DENIED` and
shall not return an empty listing. The error follows the system error, not its
presumed cause: a path that a sandbox does not expose is usually missing inside
it (`ENOENT`) and then fails with `ERR_DOES_NOT_EXIST` (REQ-012).
*Verified by:* integration with a `chmod 000` directory (skipped when run as
root); manual under Flatpak (M1) with an image outside the granted paths: a KIO
error, never a crash or an empty view, and
`docs/prototypes/m1-flatpak-plugin-path.md` notes which one.

**REQ-014** (M0b, unwanted) If the image path names a FIFO, socket or device,
then the worker shall fail with `ERR_CANNOT_OPEN_FOR_READING` without reading
from it.
*Verified by:* integration with a FIFO created by `mkfifo`.

**REQ-015** (M0b, unwanted) If the path contains a NUL byte or an ASCII control
character, then the worker shall fail with `ERR_MALFORMED_URL`.
*Verified by:* unit (resolver).

**REQ-016** (M0b, ubiquitous) The worker shall associate a cached open image
with a request only when the request path equals the image path or continues it
after a `/` separator.
*Verified by:* unit: cached `x.iso` must not serve `x.isox/...`.

**REQ-017** (M0b, event-driven) When the image file's device, inode, size or
modification time differs from the cached values, the worker shall reopen the
image before serving the request.
*Verified by:* integration: list, replace the fixture with different content,
list again.

**REQ-018** (M0b, unwanted) If the inner path names no entry, then the worker
shall fail with `ERR_DOES_NOT_EXIST`.
*Verified by:* integration.

**REQ-019** (M0b, unwanted) If the inner path continues below a regular file
that is not a supported archive, then the worker shall fail with `ERR_IS_FILE`.
*Verified by:* integration: `rr.iso/dir/file.txt/x`.

**REQ-020** (M2, optional feature) Where a volume is case-insensitive (Joliet,
HFS/HFS+, FAT, NTFS), the worker shall resolve a path component that has no exact
match to the unique entry that matches it case-insensitively, and shall fail
with `ERR_DOES_NOT_EXIST` when several entries match.
*Verified by:* unit with a FAT fixture.

## 3. Listing and stat

**REQ-021** (M0a, event-driven) When a directory inside an image is listed, the
worker shall emit one entry per child, each with `UDS_NAME`, `UDS_FILE_TYPE`
and `UDS_ACCESS`, plus `UDS_MODIFICATION_TIME` when the format records one, plus
`UDS_SIZE` for regular files.
*Verified by:* integration.

**REQ-022** (M0a, event-driven) When a directory inside an image is listed, the
worker shall emit exactly one entry named `.` describing that directory.
*Verified by:* integration (same check as kio-extras `testkioarchive`).

**REQ-023** (M0a, event-driven) When the image root is stat'ed or listed, the
worker shall describe it as a directory whose `UDS_MIME_TYPE` is the image's
MIME type and whose modification time is the image file's.
*Verified by:* integration: `stat(unpack:<rr.iso>)`.

**REQ-024** (M0a, optional feature) Where an ISO 9660 volume carries Rock Ridge
extensions, the worker shall present the Rock Ridge names, permission bits,
symbolic links, and owner and group IDs.
*Verified by:* unit + integration with fixture `rr.iso`
(`xorriso -as mkisofs -R -J`) containing a long mixed-case name, a mode-0700
file, and a relative symlink.

**REQ-025** (M0a, optional feature) Where an ISO 9660 volume carries Joliet but
no Rock Ridge, the worker shall present the Joliet names.
*Verified by:* unit with fixture `joliet.iso` made with
`xorriso -as mkisofs -J --norock` (xorriso writes Rock Ridge unless told not
to, so `-J` alone is not enough). The Joliet tree has no symlinks, and its
modes are the worker's synthesised ones (`docs/architecture.md`, section 4.4).

**REQ-026** (M0a, event-driven) When a symbolic link inside an image is listed or
stat'ed, the worker shall report it as a symbolic link with its target in
`UDS_LINK_DEST` and shall not follow it.
*Verified by:* integration.

**REQ-027** (M0b, unwanted) If an entry path in the image contains an empty
component, `.` or `..`, then the worker shall not expose that entry under a path
outside its directory.
*Verified by:* unit with a crafted tar or ISO containing `../escape`; fuzz
invariant.

**REQ-028** (M0b, unwanted) If an entry name is not valid UTF-8 after the
format's own decoding (ZIP UTF-8 flag or Info-ZIP Unicode Path field, Joliet
UCS-2, pax UTF-8), then the worker shall decode that name's raw bytes from the
legacy charset of its format: `[Names] LegacyCodepage` for ZIP and
`[Names] LegacyUnixCharset` for tar, cpio and Rock Ridge, each defaulting by
REQ-099. A name that is valid UTF-8 shall be kept as it is, also when the ZIP
UTF-8 flag is missing. The worker shall read the headers only once for this.
If the name contains bytes the charset cannot decode, or ASCII control
characters, the worker shall replace those bytes or characters with U+FFFD. If
two names in one directory become equal, the worker shall append ` (2)`,
` (3)`, … to the later ones.
*Verified by:* unit with crafted ZIPs: UTF-8 flag set; Unicode Path field; no
flag with UTF-8 names (kept); no flag with CP437 names and
`LegacyCodepage=CP437` (decoded); no flag with CP852 Polish names, system
language Polish and no key set (decoded); one archive with a flagged UTF-8 name
and an unflagged CP852 name (both right); tar with ISO-8859-2 names and system
language Polish (decoded); undecodable bytes (U+FFFD); a control character; two
entries that collide after replacement; in every case libarchive reads the
headers once (test hook counting `archive_read_open1` calls).

**REQ-099** (M0b, ubiquitous) Where `[Names] LegacyCodepage` or
`[Names] LegacyUnixCharset` is not set, the worker shall take its value from
the system language (`QLocale::system()`), and for English and Chinese also
from the territory: for ZIP the DOS/Windows OEM code page of that locale, as
7-Zip uses it, for example `CP852` for Polish and `CP850` for British
English; for Unix formats the legacy Unix charset of that locale, for example
`ISO-8859-2` for Polish; `CP437` and `ISO-8859-1` for locales not in the
table in `docs/architecture.md`, section 12, including US English.
*Verified by:* unit of the table lookup for every row of the table (`en_GB`
and `en_US`, `zh_CN`, `zh_TW` and `zh_HK` separately) and for an unknown
language; integration through REQ-028's Polish cases.

**REQ-098** (M0b, unwanted) If a ZIP entry name, after the decoding of REQ-028,
contains `\` and no `/`, then the worker shall treat every `\` in it as a
directory separator, before the rules of REQ-027 are applied.
*Verified by:* unit with crafted ZIPs whose entries are named `DIR\SUB\FILE.TXT`
with "version made by" host 0 (MS-DOS, which libarchive already converts), 11
(NTFS) and 3 (Unix): each shows `DIR/SUB/FILE.TXT` as nested folders; with a
CP437 name that needs `LegacyCodepage`; `..\..\escape.txt` stays inside the
archive root; `dir/a\b.txt` (both separators) keeps its name.

**REQ-029** (M0a, event-driven) When `stat` is requested with details that
include the MIME type, the worker shall report the MIME type of a regular file
determined from its name and its first bytes.
*Verified by:* integration with `KIO::stat(..., StatMimeType)` on a text file
named without extension.

## 4. Reading files

**REQ-030** (M0a, event-driven) When a regular file inside an image is read with
`get`, the worker shall deliver exactly the file's bytes, announce the total size
before the first data, and report a MIME type once.
*Verified by:* integration: `KIO::storedGet` content equals the source file
used to build the fixture; byte-compare for a multi-MiB file.

**REQ-031** (M0b, unwanted) If `get` targets a directory, the image root or a
volume root, then the worker shall fail with `ERR_IS_DIRECTORY`.
A nested archive (REQ-044) and `[Volumes]` (REQ-093) are not directories for
this rule.
*Verified by:* integration.

**REQ-032** (M0b, event-driven) When `get` targets a symbolic link whose target
resolves inside the same volume within 40 steps, the worker shall redirect to the
`unpack:` URL of the target.
*Verified by:* integration.

**REQ-033** (M0b, unwanted) If `get` targets a symbolic link whose target escapes
the volume, does not exist, or loops, then the worker shall fail with
`ERR_DOES_NOT_EXIST`.
*Verified by:* integration with crafted links.

**REQ-034** (M0b, unwanted) If the data of an entry is corrupt or truncated, then
the worker shall fail the `get` with `ERR_CANNOT_READ` and shall not report
success.
*Verified by:* integration with a fixture truncated in the middle of a file
extent.

**REQ-035** (M0a, ubiquitous) The worker shall never modify, create or delete
files on disk other than its own temporary files, and shall reject write
operations with `ERR_UNSUPPORTED_ACTION`.
*Verified by:* integration: `KIO::put`, `KIO::mkdir`, `KIO::del` on `unpack:`
URLs; checksum of the fixture before and after the suite.

## 5. Unsupported and broken input

**REQ-036** (M0b, unwanted) If no backend can open the image, then the worker
shall fail with `ERR_WORKER_DEFINED` and a message that names the file and states
that the format is unsupported or the data is damaged.
*Verified by:* integration with a random-bytes file named `bad.iso`; manual:
Dolphin shows the error and offers to open the file with the default
application for `application/vnd.efi.iso`.

**REQ-037** (M0b, unwanted) If a backend reports an error by throwing, then the
worker shall answer the request with a KIO error from the error table in
`docs/architecture.md` and shall keep serving later requests.
*Verified by:* unit of `guard()` with a fault-injecting backend; integration:
a failing request followed by a succeeding one in the same worker.

**REQ-038** (M0b, unwanted) If memory allocation fails while serving a request,
then the worker shall fail with `ERR_OUT_OF_MEMORY`.
*Verified by:* unit of `guard()` with an injected `std::bad_alloc`.

**REQ-101** (M0b, unwanted) If an entry's data is encrypted, then the worker
shall still list the entry when its header is readable, and `get` of it shall
fail with `ERR_WORKER_DEFINED` and a message naming the entry and stating that
password-protected archives are not supported. If the headers themselves are
encrypted, opening the archive shall fail with `ERR_WORKER_DEFINED` and the
same statement. Asking for a password is OQ-8.
*Verified by:* integration with a `zip -P` fixture: listing succeeds, `get`
fails with the message.

**REQ-102** (M0b, unwanted) If the image is one part of a multi-volume archive,
then the worker shall open only that file, shall not look for the other parts,
and shall never report success for a `get` whose data continues in another
part (REQ-034 applies). Joining parts is OQ-9.
*Verified by:* integration with the parts of a `zip -s 64k` archive opened one
by one: every request either delivers bytes identical to the source or fails
with a KIO error from the error table.

**REQ-039** (M0b, ubiquitous) The worker shall choose the backend for an image
from the image's content, independently of its file name extension.
Descriptor formats without a signature come in M3 and are recognised by their
extension (REQ-068).
*Verified by:* unit: `rr.iso` renamed to `.bin` and `.img` still opens.

**REQ-040** (M2, event-driven) When an image carries both ISO 9660 or UDF
descriptors and an MBR or GPT signature (isohybrid), the worker shall open it as
a disc image whose default volume is the ISO 9660 or UDF file system. A
partition of such an image shall be listed under `[Volumes]` only if its byte
range neither contains the ISO 9660 volume descriptors nor equals the range of
an El Torito boot image (REQ-066).
*Verified by:* unit with `xorriso -as mkisofs -isohybrid-mbr` output (no
partition listed), with `-isohybrid-gpt-basdat` and an EFI boot image given
with `-e` (the EFI image appears once, under `El Torito`), and with
`-append_partition 2 0x0c` of a FAT image that no boot catalog entry names
(listed as a partition). A zero-filled 432-byte file serves as the MBR
template; on 2026-10-07 xorriso 1.5.8 built all three images that way, and
`fdisk -l` showed partition 1 starting at sector 0 in each, plus the EFI image
and the appended FAT image as partition 2.

**REQ-041** (M2, event-driven) When the preferred backend fails to open an
image, the worker shall try the remaining backends that accept the format in
fixed priority order, and shall produce the same result on every run.
*Verified by:* unit with a fault-injecting preferred backend; repeated runs with
different `QT_HASH_SEED`.

## 6. Nested archives

**REQ-042** (M2, state-driven; the default provisional until spike S-6, OQ-13)
While `[Nested] PresentAs` is `Folder` (the default), the worker shall list
and stat an entry whose name maps to a supported archive type as a directory with `UDS_MIME_TYPE` and `UDS_ICON_NAME` of that
archive type, `UDS_SIZE` of the archive, and `UDS_DISPLAY_TYPE`
"Archive (browsable)".
*Verified by:* integration: `outer.zip` containing `disc.iso`, default config.

**REQ-043** (M2, state-driven) While `[Nested] PresentAs` is `File`, the worker
shall list and stat such an entry as a regular file with `UDS_MIME_TYPE` and
`UDS_ICON_NAME` of its archive type and `UDS_SIZE` of the archive, so that
copying it yields the archive file.
*Verified by:* integration: `KIO::copy` of `outer.zip/disc.iso` produces a file
byte-identical to the source `disc.iso`.

**REQ-044** (M2, ubiquitous) The worker shall, in both presentation modes, list
the root of a nested archive on `listDir` of its URL, resolve paths below it
inside it, and deliver the archive's raw bytes on `get` of its URL.
*Verified by:* integration: the three operations in each mode.

**REQ-045** (M2, event-driven) When a path continues below a nested archive
entry, the worker shall resolve the rest of the path inside that nested archive.
*Verified by:* integration: `get unpack:<outer.zip>/disc.iso/dir/file.txt`.

**REQ-046** (M2, event-driven) When the parent volume supports in-place access to
an entry, or the entry is stored without compression at a known offset, the
worker shall read the nested archive in place and shall not create a temporary
file.
*Verified by:* unit: `UnpackStats.tempBytes == 0` for ISO in UDF, ISO in an MBR
partition, ISO in ISO, ISO in uncompressed tar, ISO in `zip -0`.

**REQ-047** (M2, event-driven) When a nested archive is a single compressed
stream (gzip, xz, bzip2, zstd) or a deflated zip entry, the worker shall serve
listings and reads by decoding the stream forward no further than the furthest
offset requested so far plus one cache block, and shall not create a temporary
file.
*Verified by:* unit: `disc.iso.gz` and `disc.iso` in `zip -9`; assert
`UnpackStats.decodedBytes` bound and `tempBytes == 0`; listing decodes less than
the full stream for an ISO whose file data is larger than its directory area.

**REQ-048** (M2, event-driven) When a nested archive inside a deflated ZIP entry
or a gzip stream is read backwards, the worker shall resume decoding at the
nearest recorded checkpoint at or before the target, not at the start of the
stream.
*Verified by:* unit with the checkpoint interval lowered through a test hook to
1 MiB: a 16 MiB deterministic stream gzip-compressed at test time; after one
full read, a backward seek to the middle decodes at most one checkpoint
interval (`UnpackStats`). The same stream as a deflated ZIP64 entry placed at a
logical offset above 4 GiB in a `SparseByteSource`, and the checkpoint index
filled with synthetic offsets above 4 GiB, check 64-bit offsets without
decoding gigabytes. Stress (periodic job): a full read of a 512 MiB
`disc.iso.gz`, then the same backward seek.

**REQ-049** (M2, ubiquitous) Each data locator (ISO 9660, tar, ZIP) and the
checkpointing decoder shall have a libFuzzer harness that checks that every
reported byte range lies inside its source.
*Verified by:* fuzz CI job (REQ-082).

**REQ-050** (M2, unwanted) If an entry listed as a nested archive cannot be
opened by any backend, then the worker shall fail with `ERR_WORKER_DEFINED` when
the entry is entered, and `get` on the entry shall still deliver its raw bytes.
*Verified by:* integration: a text file named `fake.iso` inside a zip.

**REQ-051** (M2, unwanted) If resolving a path would open more nested archives
than the configured maximum depth (default 4), then the worker shall fail with
`ERR_WORKER_DEFINED`.
*Verified by:* unit with a five-level chain.

**REQ-052** (M2, unwanted) If a temporary file would exceed the per-file or
per-session size limit, then the worker shall stop writing it, release it, and
fail with `ERR_DISK_FULL`.
*Verified by:* unit with limits lowered through configuration.

**REQ-053** (M2, ubiquitous) The worker shall create temporary files only below
`QStandardPaths::CacheLocation` + `/kio-unpack` (or the configured directory),
as unnamed files that leave nothing behind when the worker exits for any reason.
Where the file system cannot create unnamed files, the worker shall create a
named file and remove its name before writing any data to it; only a crash
between those two steps may then leave a file behind.
*Verified by:* integration: kill the worker during a temp-backed read and assert
the directory is empty; unit with a seam that makes the unnamed-file call
fail with `EOPNOTSUPP` and then with `EISDIR`: the fallback is used, the
directory has no entry while data is written, and reads return the data.

**REQ-054** (M2, ubiquitous) kio-unpack shall install a Dolphin service menu,
shown only for single selected `unpack:` items of the supported archive MIME
types, offering the actions "Open Archive as Folder" and "Save Archive File As…"
in both presentation modes.
This is a second `.desktop` file, separate from the local menu of REQ-007,
because KIO matches `X-KDE-Protocol` exactly and shows a menu without it for
every protocol except `trash`
(`kio/src/widgets/kfileitemactions.cpp:614-639`, KIO `69f11e2c`).
*Verified by:* unit: parse the installed `.desktop` file (`X-KDE-Protocol=unpack`,
`X-KDE-RequiredNumberOfUrls=1`, MIME list equals the supported list); manual in
Dolphin, natively and in the Flatpak.

**REQ-055** (M2, event-driven) When the user chooses "Open Archive as Folder",
Dolphin shall show the listing of the nested archive's root.
*Verified by:* manual in `PresentAs=File` mode; integration of
`kio-unpack-action open-as-folder` with a stub launcher capturing the command.

**REQ-056** (M2, event-driven) When the user chooses "Save Archive File As…" and
confirms a destination, the raw archive file shall be written there without
extracting its contents.
*Verified by:* integration of `kio-unpack-action save-archive` with the
destination passed on the command line (test mode): output byte-identical to the
source archive; `UnpackStats` shows no nested open.

**REQ-057** (M2, state-driven) While `[Nested] PresentSolidAs` is `File` (the
default), the worker shall list and stat a nested archive inside a 7z or RAR
parent as a regular file, regardless of `[Nested] PresentAs`. libarchive
reports neither which entries share a solid block nor how an entry is
compressed, so every 7z or RAR entry counts as solid (ADR 0006).
*Verified by:* integration with `PresentAs=Folder`: `disc.iso` as the second
file of a solid `7z -ms=on` archive and of a non-solid `7z -ms=off` archive,
both listed as regular files. No RAR fixture: no Fedora package provides a
`rar` command (`dnf repoquery --whatprovides /usr/bin/rar`, 2026-10-07), so
the RAR case is a unit test of the presentation rule on the format.

**REQ-058** (M2, event-driven; provisional until spike S-3, OQ-6) When the user browses into a nested archive covered by
REQ-057 and the bytes to decode exceed `[Nested] ConfirmCostlyAboveMiB`
(default 64), the worker shall ask for confirmation with a continue/cancel
message box stating the file name, the amount of data to decode and the temporary
space needed, offering "do not ask again". The amount to decode is an upper
bound: the uncompressed sizes of the entry and of every entry before it in the
archive.
*Verified by:* integration with a test UI delegate that records the message box
and answers it; manual in Dolphin.

**REQ-059** (M2, unwanted; provisional until spike S-3, OQ-6) If the user cancels that confirmation, then the
worker shall fail the request with `ERR_USER_CANCELED` without decoding the block.
*Verified by:* integration (`UnpackStats.decodedBytes == 0` after cancel).

**REQ-060** (M2, unwanted; provisional until spike S-3, OQ-6) If the confirmation cannot be shown, then the worker
shall proceed and report the same text through a KIO warning.
*Verified by:* integration with a client that has no UI delegate.

## 7. Volumes and hybrid images

**REQ-061** (M2, ubiquitous) The worker shall show at the image root the
contents of the default volume, chosen in the order UDF, ISO 9660 with Rock
Ridge, ISO 9660 with Joliet, plain ISO 9660.
*Verified by:* unit with fixtures: Rock Ridge + UDF (`genisoimage -R -udf`;
xorriso has no UDF output) shows UDF and lists Rock Ridge under `[Volumes]`;
`mkudffs` image shows UDF; Rock Ridge-only image shows Rock Ridge.

**REQ-062** (M2, state-driven) While an image contains more than one volume or
more than one name flavour, the worker shall list at the image root a directory
named `[Volumes]` whose children are the volumes by label.
*Verified by:* integration with `genisoimage -R -J -hfs` fixture.

**REQ-063** (M2, state-driven) While an image contains exactly one volume with
one name flavour, the worker shall not list a `[Volumes]` directory.
*Verified by:* integration with a plain `-R` fixture without Joliet.

**REQ-064** (M2, unwanted) If the default volume contains an entry named
`[Volumes]`, then the worker shall name the virtual directory `[Volumes~N]` with
the smallest N ≥ 1 that does not collide.
*Verified by:* unit.

**REQ-092** (M2, ubiquitous; provisional until spike S-4, OQ-11) The worker shall report `[Volumes]` in listings and
in `stat` as a directory with `UDS_LINK_DEST` set and the display type
"Volumes (virtual folder)", and shall resolve
`[Volumes]` only at an image root, so that a recursive listing or copy of the
image root does not descend into it (ADR 0008).
*Verified by:* integration with a Rock Ridge + Joliet + UDF fixture:
`KIO::listRecursive` of the image root returns no entry below `[Volumes]`;
`KIO::copy` of the image root into a temporary directory yields the default
volume's files once and one regular file `[Volumes]`; `KIO::copy` with
`[Volumes]` itself as a source, alone and together with other root entries,
yields one regular file `[Volumes]` and no tree;
`[Volumes]/UDF/[Volumes]` gives `ERR_DOES_NOT_EXIST`.

**REQ-093** (M2, event-driven; provisional until spike S-4, OQ-11) When `get` targets `[Volumes]`, the worker shall
deliver a UTF-8 text with MIME type `text/plain` that starts with one translated
sentence saying that the folder is virtual and lists every volume label on its
own line.
*Verified by:* integration: `KIO::storedGet` on `[Volumes]` returns the labels
reported by `listDir` on it, and `UDS_SIZE` from `stat` equals the text's
length.

**REQ-065** (M2, optional feature) Where an image contains an HFS or HFS+
volume, found through an `Apple_HFS` entry of an Apple partition map or through
an HFS or HFS+ signature at offset 0x400 of the image, the worker shall list it
once under `[Volumes]` (both ways of finding the same start offset name one
volume) with display type "HFS (not supported)" or "HFS+ (not supported)" and
shall fail with `ERR_WORKER_DEFINED` when it is entered. Other partition map
entries of a disc image follow REQ-040.
*Verified by:* integration with `genisoimage -hfs` (HFS); HFS+ with a fixture
whose ISO 9660 system area receives the first 32 KiB of a volume made by
`mkfs.hfsplus` (hfsplus-tools, which works on a plain file without root).

**REQ-066** (M2, optional feature) Where an image has an El Torito boot catalog,
the worker shall list each boot image as a regular file under
`[Volumes]/El Torito`; `El Torito` is a volume whose root holds only these
files, and `get` delivers the bytes of the boot image. File names of the boot
images are not specified yet (OQ-12).
*Verified by:* unit with `xorriso -as mkisofs -b boot.img -no-emul-boot` and
`-eltorito-alt-boot` fixtures.

**REQ-067** (M2, optional feature) Where a volume is UDF, the worker shall list
entries with name, type, size, modification time, permissions, owner and group,
report symbolic links with their targets, and read files with random access.
*Verified by:* unit with an empty `mkudffs` image and an image made with
`genisoimage -udf` (xorriso has no UDF output: `-as mkisofs: Unsupported
option '-udf'`), including a mode-0700 file and a symlink. Large files: a
fixture generator in `tests/` patches a small `genisoimage -udf` image so that
one file entry declares 4.5 GiB in several extents placed at logical blocks
beyond 4 GiB, with tag checksums and CRCs recomputed (ECMA-167); a
`SparseByteSource` serves the real image bytes and a deterministic pattern
computed from the offset for the data extents, so reads around the 4 GiB
boundary are checked without writing 4.5 GiB. If libudfread rejects the patched
image, this case runs only in the stress job. Stress (periodic job): a real
4.5 GiB file in a `genisoimage -udf -allow-limited-size` image (UNVERIFIED that
genisoimage writes UDF files of that size; the job checks it).

## 8. Optical images

**REQ-068** (M3, optional feature; required for 1.0) Where an image is a CUE
sheet with its BIN file or files, an MDS descriptor with its MDF file, a CCD
descriptor with its IMG (and optional SUB) file, or a TOC file with its data
files, the worker shall present each data track's file system (ISO 9660 through
libarchive, UDF through libudfread) and each audio track as a WAV file
(REQ-071). Data files are found next to the descriptor in the same directory or
archive directory, by exact name first and then case-insensitively; names that
the descriptor stores with a path are resolved by REQ-097. Descriptor files
carry no signature and are recognised by their extension (`.cue`, `.ccd`,
`.mds`, `.toc`), the one exception to REQ-039.
*Verified by:* unit with generated fixtures for each descriptor format (single
data track MODE1/2352, MODE2/2352, mixed mode with audio, multiple FILE entries,
CUE+BIN inside a ZIP); corpus run over the BIN/CUE and CCD samples
(`22000gifs`, `Audio CD`, `WARCRAFT2_X`, `Nesquik-QuickyEuro`).

**REQ-097** (M3, unwanted) If a descriptor (CUE `FILE`, TOC `FILE` or
`DATAFILE`, MDS) names a companion file with a path, then the worker shall
treat `\` and `/` as separators and:
- for a relative path without `..` components, try that path below the
  descriptor's directory, then its last component in the descriptor's
  directory;
- for an absolute path (drive letter such as `C:`, leading separator, UNC
  `\\server\share`) or a path with `..` components, use only its last
  component in the descriptor's directory;
- never open a file outside the descriptor's directory and its subdirectories,
  judged on disk by the real path after resolving symlinks and inside an
  archive without following archive symlinks;
- match each candidate by exact name first, then case-insensitively;
- if no candidate exists, fail with `ERR_WORKER_DEFINED` and a message naming
  the missing file.
*Verified by:* unit with CUE fixtures, on disk and inside a ZIP:
`FILE "C:\GAMES\WAR2\WAR2.BIN" BINARY` with `war2.bin` next to the sheet;
`FILE "DATA\TRACK01.BIN" BINARY` with the file in `DATA/` and, separately, only
next to the sheet; `FILE "/etc/passwd" BINARY` and `FILE "..\x.bin" BINARY`
with a matching file outside the sheet's directory, which must not be opened;
`FILE "DATA\X.BIN" BINARY` where `DATA` is a symlink to a directory outside;
a missing file gives the error with its name.

**REQ-069** (M3, ubiquitous) The worker shall read data and audio tracks stored
with any of the sector sizes 2048, 2056, 2324, 2332, 2336, 2340, 2352 and 2448
bytes, taking user data from the correct offset for Mode 1, Mode 2 Form 1 and
Mode 2 Form 2 sectors and ignoring 96-byte subchannel data, and shall detect the
sector size of raw dumps without a descriptor.
*Verified by:* unit: one ISO 9660 payload converted into each sector layout by a
test helper, as a raw dump and with a CUE sheet; every variant lists and reads
identically.

**REQ-070** (M3, unwanted) If a file on a data track lies in Mode 2 Form 2
sectors (CD-XA subheader with the Form 2 bit set, as used by Video CD `.DAT`
files and PlayStation `.STR` streams) and the worker cannot deliver its Form 2
payload, then `get` shall fail with `ERR_CANNOT_READ` and a message naming the
reason, and shall never deliver data assembled from 2048-byte user-data areas.
Milestones before M3 read only 2048-byte-sector images and add no Form 2
handling of their own.
*Verified by:* unit with a Video CD BIN/CUE generated by `vcdimager` from a short
MPEG-1 file (made with `ffmpeg`); the `.DAT` file either matches the source MPEG
payload or fails with the stated error.

**REQ-071** (M3, optional feature) Where an optical image contains audio
tracks, the worker shall present each audio track as a WAV file named
`Track NN.wav` (RIFF header, 16-bit stereo PCM at 44.1 kHz, little-endian, with
byte order corrected for big-endian sources and subchannel data removed) whose
size equals header plus the track's audio bytes, and `get` shall deliver a valid
WAV stream. On an image without data tracks the WAV files are listed directly at
the image root, without `[Volumes]`; on a mixed-mode image they are listed under
`[Volumes]`.
*Verified by:* unit with generated CUE/BIN fixtures: audio-only (root lists
`Track 01.wav`, `Track 02.wav`, no `[Volumes]`) and mixed mode (WAV files under
`[Volumes]`); `MOTOROLA` byte order and 2448-byte sectors; the WAV header is
checked field by field and its PCM payload matches the source.

**REQ-072** (M3, ubiquitous) The worker shall not declare `application/x-cue` in
its `archiveMimetype` list, so a local `.cue` keeps opening with the user's
default application; nor `application/octet-stream`, the type `.bin` files get.
*Verified by:* unit over the embedded JSON.

**REQ-073** (M3, event-driven) When the user chooses "Open as Folder
(kio-unpack)" on a single local `.cue` file in Dolphin, the view shall show the
image's root as an `unpack:` folder (ADR 0007).
*Verified by:* unit: the local service menu lists `application/x-cue`;
integration of `kio-unpack-action open-as-folder` with a stub launcher; manual in
Dolphin.

**REQ-074** (M3, state-driven) While a `.cue` file lies inside an archive or
image, the worker shall treat it as a nested archive under `[Nested] PresentAs`
(REQ-042, REQ-043).
*Verified by:* integration: CUE+BIN inside a ZIP, both presentation modes.

**REQ-075** (M3, unwanted) If a CUE sheet makes several tracks share one
compressed audio file (`FLAC`, `MP3`, `OGG`, `APE` or any file that is neither raw
sector data nor uncompressed PCM WAV), then the worker shall fail with
`ERR_WORKER_DEFINED` and a message stating that splitting compressed audio albums
into tracks is not supported.
*Verified by:* unit with `album.cue` + one `album.flac` for all tracks (error).

**REQ-076** (M3, event-driven) When every compressed audio file referenced by a
CUE sheet belongs to exactly one track, the worker shall list each such file
unchanged under its original file name, deliver its bytes as they are, and shall
not report an error. Uncompressed PCM WAV files are presented as WAV tracks by
mapping their PCM data (REQ-071).
*Verified by:* unit with `album.cue` + `01.mp3`, `02.mp3` (both listed with
original names, bytes identical to the sources) and `album.cue` + `album.wav`
(tracks listed).

## 9. Robustness

**REQ-077** (M0b, event-driven) When the client cancels a running `listDir` or
`get`, the worker shall stop the operation after at most one further data chunk
(starting value 1 MiB, architecture section 9.1) or directory entry and report
`ERR_USER_CANCELED`.
*Verified by:* unit: an extraction loop whose kill flag is set after the first
chunk delivers at most one more chunk; the same for a listing loop and
entries; integration: kill a `get` of a large file and assert that the worker
process has exited, or serves the next request, well before KIO's own kill five
seconds after `SIGTERM` (`kio/src/core/slavebase.cpp:229-243`).

**REQ-078** (M0c, unwanted) If an open or list operation exceeds its deadline, or
an extraction delivers no data for its stall deadline, then the worker shall end
its process with `_exit()` so that the client receives `ERR_WORKER_DIED` and no
crash report is produced.
*Verified by:* integration with a fault-injecting backend that blocks,
deadlines lowered through configuration; assert error code and no core dump /
DrKonqi invocation.

**REQ-096** (M0c, state-driven) While the worker waits for the next request, or
waits for the client inside an operation (sending data or entries, showing a
message box), the worker shall not end its process because of an operation
deadline.
*Verified by:* integration with deadlines lowered to 1 s: the worker stays idle
for 3 s, then serves `stat` and `listDir` on the same process; a `get` whose
client suspends the job for 3 s completes with the right bytes (provisional
until spike S-2); from M2, a REQ-058 message box answered after 3 s continues
the operation (provisional until spike S-3).

**REQ-079** (M0c, ubiquitous) The worker shall limit its data segment with
`RLIMIT_DATA` to the configured value (default 2048 MiB), except in sanitizer
builds.
*Verified by:* unit of the limit setup; integration: `special()` debug command
reporting `getrlimit` (test builds only).

**REQ-094** (M2, unwanted) If a write to a temporary file fails because it
reaches `RLIMIT_FSIZE`, then the worker shall keep running, release the file and
fail with `ERR_DISK_FULL`.
*Verified by:* integration with `RLIMIT_FSIZE` lowered through configuration
below the temporary-file cap, so the user-space check of REQ-052 does not
trigger first; assert `ERR_DISK_FULL`, a following request on the same worker
succeeds, and the temporary file is gone.

**REQ-095** (M0c, unwanted) If the worker exceeds its soft `RLIMIT_CPU`, then it
shall end its process with `_exit()` so that the client receives
`ERR_WORKER_DIED` and no crash report or core dump is produced.
*Verified by:* integration with the CPU limit lowered through configuration and
a fault-injecting backend that spins; assert the error code and no core dump /
DrKonqi invocation, as for REQ-078.

**REQ-080** (M0c, unwanted) If indexing a volume would exceed the configured
entry limit (default 2,000,000) or string arena limit (default 512 MiB;
architecture section 9.1), then the worker shall stop indexing and fail with
`ERR_WORKER_DEFINED`.
*Verified by:* unit with the limit lowered.

**REQ-081** (M0b, event-driven) When the worker has been idle for the idle
period (`kio_unpackrc` `[Limits] IdleSeconds`, default 90 seconds, architecture
section 9.1), it shall close all open images and release their memory and
temporary files.
*Verified by:* unit of `SessionCache` with an injected clock; integration with
`IdleSeconds` lowered in a `kio_unpackrc` that the test writes into the
worker's configuration directory.

**REQ-082** (M0c, ubiquitous) The worker shall be buildable as libFuzzer
harnesses for sniffing, for every backend and for every parser of our own
(partitions, El Torito, data locators, checkpointing decoder, optical
descriptors), with AddressSanitizer and UndefinedBehaviorSanitizer enabled.
A parser added after M0c gets its harness in the same change.
*Verified by:* CI build with `-DKIO_UNPACK_FUZZ=ON` and a 60-second run per
harness without findings, on every push from M0c on.

## 10. Configuration

**REQ-083** (M0c, optional feature) Where `kio_unpackrc` sets keys in group
`[Limits]` (`MemoryLimitMiB`, `MaxEntries`, `MaxArenaMiB`, `OpenTimeoutSec`,
`ListTimeoutSec`, `StallTimeoutSec`), the worker shall use those values instead
of the defaults. The other keys arrive with the features they control:
`[Limits] IdleSeconds` (REQ-081) and the `[Names]` keys (REQ-028, REQ-099) in
M0b; `[Limits] MaxNestingDepth`, `MaxTempFileMiB` and `MaxTempTotalMiB`
(REQ-051, REQ-052) in M2.
*Verified by:* unit of the config loader; integration via
`QStandardPaths::setTestModeEnabled` config.

**REQ-084** (M2, optional feature) Where `kio_unpackrc` sets
`[Nested] PresentAs` to `Folder` or `File`, the worker shall present nested
archives accordingly (REQ-042, REQ-043), re-reading the value on
`reparseConfiguration()` and at worker start; any other value shall be treated
as `Folder`.
*Verified by:* unit of the config loader; integration in both modes.

**REQ-085** (M2, optional feature) Where `kio_unpackrc` sets `[Temp] Dir`, the
worker shall create temporary files there.
*Verified by:* unit.

**REQ-086** (M2, optional feature) Where `kio_unpackrc` sets
`[Nested] PresentSolidAs` (`File` or `Folder`) or `[Nested] ConfirmCostlyAboveMiB`,
the worker shall use those values.
*Verified by:* unit of the config loader.

## 11. Build and packaging

**REQ-087** (M0a, ubiquitous) The source tree shall contain no hardcoded `/usr`
path in code or build files; installation paths shall come from CMake
installation variables or configuration.
*Verified by:* `ctest` grep check over `src/`, `tests/`, `prototypes/`,
`scripts/`, `.github/` and the top-level `CMakeLists.txt` (documentation and the
upstream Flatpak files describe systems and are not checked; a `#!` line at
the top of a script names an interpreter and is allowed); M1 Flatpak build with
prefix `/app`.

**REQ-088** (M1, ubiquitous) The worker and all its bundled dependencies shall
build with flatpak-builder against org.kde.Sdk without network access in the
build sandbox.
*Verified by:* M1 build of `flatpak/org.kde.dolphin.json`.

**REQ-089** (M1, state-driven) While running inside the Dolphin Flatpak, the
worker shall be discovered at `/app/lib/plugins/kf6/kio/kio_unpack.so` and serve
images inside the paths granted to Dolphin.
*Verified by:* manual + `kioclient` inside the sandbox (M1 acceptance).

**REQ-103** (M0a, ubiquitous) kio-unpack shall build a command-line tool
`kio-unpack-cli` with the commands `list`, `stat` and `cat`, which uses
`libkio-unpack-core` without KIO and reports the same entries and bytes as the
worker. It is a development tool (smoke tests, corpus runs, fuzz seeds) and is
not installed.
*Verified by:* smoke test in `ctest`: `list` and `cat` on `rr.iso` match the
worker's `listDir` and `get`.

## 12. Localisation

**REQ-090** (M0b, ubiquitous) The worker and the core shall produce every
user-visible string through KI18n with the translation domain `kio6_unpack`.
*Verified by:* `scripts/extract-messages.sh` finds every string (M0b); from
M0c, `scripts/lint.sh` also fails on untranslated literals passed to
`WorkerResult::fail`.

**REQ-100** (M2, ubiquitous) The helper `kio-unpack-action` shall produce every
user-visible string through KI18n with the translation domain `kio6_unpack`.
*Verified by:* as REQ-090, over `src/action/`.

**REQ-091** (M0c, ubiquitous) kio-unpack shall ship a Polish translation of all
its strings, including the service-menu entries.
*Verified by:* integration with `LANGUAGE=pl`: an `ERR_WORKER_DEFINED` message is
Polish; unit: the service-menu `.desktop` file has `Name[pl]` for every action;
`msgfmt --check` on `po/pl/kio6_unpack.po`.

## Open questions

These points are deliberately not specified yet. Ideas deferred beyond 1.0 are
listed in `docs/roadmap.md`, not here. Each question states what applies until
it is decided: a requirement, a spike's fallback, or the rule that the code
depending on it is not merged before the decision. Code that meets an open
question follows that statement and does not invent other behaviour.

- **OQ-1** Which further MIME types to claim in `archiveMimetype` for double-click
  in Dolphin: the canonical types of MDS, CCD, TOC, raw `.img` dumps and
  compressed images such as `.iso.gz`, collected with `xdg-mime query filetype`
  over the corpus and checked against REQ-003, REQ-004 and ADR 0007.
  Types also used outside disc images get only the context-menu action.
  Until decided: only the types REQ-002 and the current `unpack.json` list
  are claimed; every other type is reachable through REQ-007's action.
- **OQ-2** How KIO's scheduler assigns `unpack:` jobs, which have no host, to
  workers; decides whether `maxInstances` should exceed 1. Spike S-1 at the
  start of M0b (`docs/roadmap.md`, "Spikes before dependent work"). Until
  decided: the spike's fallback, `maxInstances` 1 and the starting
  `SessionCache` size of `docs/architecture.md`, section 9.1.
- **OQ-3** Length of El Torito "no emulation" boot images whose catalog entry
  gives a sector count of 0 or 1. Decided with the El Torito parser in M2;
  the parser is not merged before.
- **OQ-4** Building libarchive and libudfread with sanitizers, so fuzzing covers
  the whole stack and not only our code. Until decided: only our code is
  instrumented (section 12 of `docs/architecture.md`); no observable
  behaviour depends on it.
- **OQ-5** Backward-seek budget, checkpoint spacing and cache size for decoded
  streams before falling back to a temporary file. Settled by measurement in
  M2 before `DecodingByteSource` is merged (`docs/architecture.md`,
  section 9.1); tests set their own values.
- **OQ-6** Whether Dolphin's directory lister gives list jobs a UI delegate that
  can show a worker's message box; ADR 0006 falls back to a warning. Spike S-3
  at the start of M0b; REQ-058 to REQ-060 are provisional until then.
- **OQ-7** Whether to deliver Mode 2 Form 2 files correctly (Video CD `.DAT`,
  CD-i, PlayStation `.STR`). Such sectors carry 2324 bytes of data, so a file-system
  reader that sees only 2048-byte user data returns wrong content; REQ-070
  forbids silent corruption in the meantime. Full support means detecting XA
  Form 2 extents and serving them from raw sectors of our track layer. Test
  material: `vcdimager` fixtures; no real Video CD or PlayStation samples are
  visible in the sembiance corpus listing.
- **OQ-8** Password-protected archives (ZIP, 7z, RAR): whether and how to ask for
  a password through KIO and cache it for the session. Until decided, REQ-101
  applies.
- **OQ-9** Multi-volume archives (`.7z.001`, `.part1.rar`, split ZIP): whether
  to join volumes found next to the first one. Until decided, REQ-102 applies.
- **OQ-10** Whether an app-level `--env=QT_PLUGIN_PATH` in a Flatpak manifest
  overrides the runtime's `[Environment]` value, which an extension point for
  KIO workers needs (ADR 0010, M4). Spike S-5 in M1. No worker behaviour
  depends on it; until decided, M4 proposes route (a).
- **OQ-11** Whether Dolphin, end to end, enters an entry that a non-local
  worker reports as `S_IFDIR` with `UDS_LINK_DEST` on double-click and lists it
  at the entry's own URL, as `KFileItem::targetUrl()` suggests (ADR 0008); and
  which `UDS_LINK_DEST` value Dolphin and the properties dialog handle best.
  Spike S-4 at the start of M0b, check C8 in M1 (`docs/roadmap.md`, result in
  `docs/prototypes/m0b-volumes-link.md`): REQ-092 and REQ-093 are confirmed, or
  ADR 0008's fallback replaces them.
- **OQ-12** File names of El Torito boot images under `[Volumes]/El Torito`
  (REQ-066): a catalog entry has a platform, an emulation type and a load
  segment but no name. Decided with the El Torito parser in M2; the parser
  is not merged before.
- **OQ-13** How much Dolphin's previews read through `unpack:`. With `Class`
  `:local` KIO applies the local preview size limit (unlimited by default)
  and makes folder previews, so opening a folder inside an image may `get`
  every previewable file and list every subdirectory, including nested
  archives shown as folders (`kio/src/gui/filepreviewjob.cpp:262-271`, KIO
  `69f11e2c`). Also how applications receive a file opened from inside an
  image. Spike S-6 at the start of M0b (`docs/roadmap.md`); the
  `[Nested] PresentAs` default (REQ-042) is provisional until then.
