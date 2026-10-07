<!--
SPDX-License-Identifier: GPL-2.0-or-later
SPDX-FileCopyrightText: 2026 kio-unpack contributors
-->

# M0a specification review

Date: 2026-10-07. Done after the M0a gate passed locally and before the first
push, as `docs/roadmap.md` (M0a, "Specification review") requires; ADR 0001
allows editing records in place until then.

Environment: toolbx container `kio-unpack` (Fedora 44): KF 6.30.0, Qt 6.11.2,
ECM 6.30.0, libarchive 3.8.7, xorriso 1.5.8, GCC 16.2.1, CMake 4.3.0,
kde-cli-tools 6.7.5, Dolphin 26.08.1.

Each statement that M0a's code touched is listed with its result: confirmed
(the code and tests behave as written) or changed (the text was corrected,
and where).

## Plugin build and discovery

| Statement | Where | Result |
|---|---|---|
| `kcoreaddons_add_plugin(kio_unpack INSTALL_NAMESPACE "kf6/kio")` puts the plugin in `build/bin/kf6/kio/` | architecture §11, ADR 0009 | confirmed: `build/bin/kf6/kio/kio_unpack.so` |
| KIO finds the worker through `QT_PLUGIN_PATH` at the build tree, without installing | ADR 0009, architecture §2 | confirmed: integration tests, smoke test and the `AGENTS.md` commands |
| `ecm_add_test` sets `QT_PLUGIN_PATH` to the build tree | architecture §13 | confirmed (`ECMAddTests.cmake`, ECM 6.30); the smoke test, added with `add_test`, sets it itself |
| Metadata embedded with `Q_PLUGIN_METADATA(... FILE "unpack.json")` | architecture §11 | changed: §11 now says the macro sits in a `KIO::WorkerFactory` subclass as in `kio_file`, and that the plugin also exports `kdemain` |
| `unpack.json` as shown in §11 | architecture §11 | changed: the write capabilities (`writing`, `deleting`, `makedir`, `moving`, `linking`) are declared `false` explicitly, and the JSON in §11 shows them |
| `Class` `:local`, `input`/`output` `filesystem`, `reading` (REQ-001) | requirements | confirmed by `integration-protocolinfotest` |
| `application/vnd.efi.iso` first; `protocolForArchiveMimetype` returns `unpack` (REQ-002) | requirements, ADR 0007 | confirmed |
| REQ-003 and REQ-004 "Verified by: unit over the embedded JSON" | requirements | changed: verified through `KProtocolInfo::archiveMimetypes("unpack")`, which reads the metadata embedded in the built plugin, so the test is an integration test |
| Write operations fail with `ERR_UNSUPPORTED_ACTION`, the WorkerBase default (REQ-035) | requirements, architecture §10 | confirmed for `put`, `mkdir` and `file_delete`; KIO passes them to the worker although the metadata declares no writing |

## Build settings (ADR 0014)

| Statement | Where | Result |
|---|---|---|
| `KDE_COMPILERSETTINGS_LEVEL` 6.13 brings C++20, the KDE warning set and the Qt definitions | ADR 0014 | confirmed (compile command lines) |
| The level cannot be newer than the ECM version required by `find_package(ECM)` | ADR 0014 (implied) | confirmed (`KDECompilerSettings.cmake:237-238`); the build requires ECM 6.13 |
| `KDECompilerSettings` turns exceptions off by default | not stated before | changed: ADR 0014, decision 2, now says that the top-level `CMakeLists.txt` calls `kde_enable_exceptions()`, because the core reports failures by throwing |
| `ecm_set_disabled_deprecation_versions(QT <min> KF <min>)` | ADR 0014 | confirmed with Qt 6.8.0 and KF 6.13.0. `KIO::UDSEntry::reserve()` is deprecated since KF 6.29 and its replacements do not exist in 6.13, so the worker does not call either |
| `kde_clang_format` generates `.clang-format` at configure time | ADR 0014 | confirmed; the file is ignored by git |
| Minimum versions Qt 6.8.0, KF 6.13.0, libarchive 3.3.3 | `CMakeLists.txt` | UNVERIFIED against org.kde.Sdk 6.10; checked by the M1 build |
| Configure prints `No such remote 'origin'` | not documented | harmless: `KDECMakeSettings.cmake` looks up the git remote; the message ends once the GitHub remote exists |

## Worker start-up (architecture §9)

| Statement | Where | Result |
|---|---|---|
| `kdemain` constructs a `QCoreApplication` and never calls `exec()`; requests run in KIO's blocking dispatch loop | architecture §9 | confirmed |
| `kioclient --platform offscreen` needs no session bus | roadmap, M0a gate | confirmed in the toolbx container with `DBUS_SESSION_BUS_ADDRESS`, `DISPLAY`, `WAYLAND_DISPLAY` and `XDG_RUNTIME_DIR` unset; in the CI container see "CI" below |

## libarchive's ISO 9660 reading (architecture §4.4)

| Statement | Where | Result |
|---|---|---|
| The archive is read through `archive_read_set_*_callback` and `archive_read_open1` over a `ByteSource` | architecture §4.4 | confirmed; only `archive_read_support_format_iso9660` is enabled in M0a |
| Default options give Rock Ridge, otherwise Joliet, otherwise plain ISO 9660 (REQ-024, REQ-025) | architecture §4.4, ADR 0002 | confirmed: `rr.iso` (`-R -J`) lists Rock Ridge names, modes 0700/0644, owner 1234/5678 and the symlink; `joliet.iso` (`-J --norock`) lists the long Joliet names without symlink |
| `archive_format()` is `ARCHIVE_FORMAT_ISO9660_ROCKRIDGE` only when Rock Ridge was used | architecture §4.4 | confirmed; the backend reads it after the first header |
| Without Rock Ridge libarchive reports 0700/0400 and owner 0; the backend synthesises modes | architecture §4.2, §4.4 | confirmed; the synthesised values were not written down. Changed: §10 now gives 0555 for directories and 0444 for files when the format records no permissions |
| `extract()` reopens the archive and walks the headers with `archive_read_data_skip` up to the ordinal | architecture §4.4 | confirmed; a 3 MiB file is delivered byte-identical |
| Hard links share data | not stated | added in code: an entry that libarchive reports with `archive_entry_hardlink()` reads the data of its target; no fixture exercises it yet |

## Path resolution (architecture §6, M0a list)

| Statement | Where | Result |
|---|---|---|
| Stat-walk selects the shortest prefix that is not a directory (REQ-009) | architecture §6 | confirmed by `unit-pathresolvertest` with on-disk names repeated inside the image |
| `//` collapsed, trailing `/` dropped (REQ-011) | architecture §6 | confirmed |
| Exact lookup, directories descend, symlinks never followed (REQ-026) | architecture §6 | confirmed; a path below a symlink gives `ERR_DOES_NOT_EXIST` |
| Every failure becomes a KIO error | architecture §4, contract 3 | confirmed for M0a's paths. Until M0b: a path that is a directory on disk gives `ERR_WORKER_DEFINED` instead of the redirect of REQ-010, and `get` on a symlink gives `ERR_CANNOT_OPEN_FOR_READING` instead of the redirect of REQ-032 |
| Empty, `.` and `..` components are dropped (ADR 0011, REQ-027 in M0b) | ADR 0011 | done already in M0a by the entry-table builder (unit test without a crafted archive); REQ-027's crafted-archive test stays in M0b |

## UDS fields and errors (architecture §10)

| Statement | Where | Result |
|---|---|---|
| Image root: `S_IFDIR`, the image's MIME type, size and mtime of the file, `UDS_DISPLAY_NAME` (REQ-023) | architecture §10 | confirmed for `stat` and for the `.` entry of the root listing |
| Exactly one `.` entry per listing (REQ-022) | requirements | confirmed; KIO adds no `..` |
| `UDS_USER`/`UDS_GROUP` only when the format records them | architecture §10 | confirmed; changed: §10 now says the IDs are reported as decimal numbers |
| `get()` announces the size, sends fixed-size chunks, the MIME type once, an empty `data()` at the end (REQ-030) | architecture §10 | confirmed; chunk size is one named constant (§9.1) |
| `stat` with `StatMimeType` sniffs the first 64 KiB (REQ-029) | architecture §10 | confirmed: `noext` gives `text/plain` |

## Translations (ADR 0013)

| Statement | Where | Result |
|---|---|---|
| "the core uses `ki18n` through a thin wrapper so it stays free of widget dependencies" | ADR 0013 | changed: KI18n links only Qt Core, so the core calls `i18n()` directly; ADR 0013 says so |

## Dev loop (ADR 0009)

| Statement | Where | Result |
|---|---|---|
| The `AGENTS.md` blocks "[M0a] Smoke test" and "[M0a] Check that KIO discovers the worker" work as written | `AGENTS.md` | confirmed in the toolbx container |
| Fixtures are "generated at test time" | `AGENTS.md`, architecture §13 | changed: they are generated by the build into `build/tests/fixtures`, because the smoke and dev-loop commands use them right after `cmake --build`; both places now say so |
| Dolphin from the build tree opens `rr.iso` on double-click | ADR 0009, REQ-006 | confirmed by the maintainer's checklist, section "Dolphin checklist" |

## Build and packaging

| Statement | Where | Result |
|---|---|---|
| REQ-087 "Verified by: CI grep check excluding `docs/` and `flatpak/`" | requirements | changed: the check runs in `ctest` over `src/`, `tests/`, `prototypes/`, `scripts/`, `.github/` and the top-level `CMakeLists.txt`, because `AGENTS.md` mentions `/usr` in prose; a `#!` line at the top of a script is allowed |
| `kio-unpack-cli` reports the same entries and bytes as the worker (REQ-103) | requirements | confirmed by the smoke test |

## CI

The GitHub Actions job was replayed before the first push in a fresh
`fedora:44` container (podman, as root, no session bus, no display), with the
package list from `scripts/toolbox-setup.sh --ci --print-packages` and the
three commands of the workflow.

- First run: every test passed except the smoke test, because the minimal
  image has no `cmp` or `diff`. Changed: `diffutils` joined
  `docs/building.md` and `scripts/toolbox-setup.sh`.
- Second run: all 9 tests passed, including the `kioclient` smoke test and
  the integration tests, which start out-of-process workers. The roadmap's
  contingency (`dbus-run-session`) is not needed; the statement in the M0a
  gate that `kioclient` needs no session bus is confirmed for the CI
  container too.

On GitHub, the first push ran the workflow (run `37687331866`, commit
`f26604b`): all 9 tests passed. GitHub annotated that `actions/checkout@v4`
and `actions/upload-artifact@v4` target the deprecated Node.js 20 and run on
Node.js 24; this does not fail the job.

## Dolphin checklist

Run by the maintainer on 2026-10-07 with the "[M0a] Dev loop" commands of
`AGENTS.md`, in the toolbx container: Dolphin 26.08.1, KIO 6.30.0, Qt 6.11.2.

| Check | Result |
|---|---|
| A double-click on `rr.iso` opens it as a folder | pass |
| Rock Ridge names, permissions and the symlink are shown | pass (no problem reported) |
| A file copied out is byte-identical | pass: `big.bin` copied by dragging from the left to the right panel, and by Ctrl+C and Ctrl+V, into `build/tests/fixtures`; `cmp` with `rr-src/big.bin` reports no difference |

Note: the first drag of `big.bin` produced a link file
`unpack:⁄⁄⁄…⁄rr.iso⁄big.bin.desktop` (`Type=Link`) instead of a copy. KIO
creates such a file only in `CopyJob`'s Link mode
(`src/core/copyjob.cpp:1014-1037`, KIO `v6.30.0`, `78260b2e`), and
`DropJob` offers "Copy Here" whenever the source protocol supports reading
(`src/widgets/dropjob.cpp:537-548`), so the drop most likely went through
"Link Here" in the drop menu. The second drag and the clipboard both
copied; the link was not reproduced.
