<!--
SPDX-License-Identifier: GPL-2.0-or-later
SPDX-FileCopyrightText: 2026 kio-unpack contributors
-->

# 0010. Flatpak build and distribution path

Date: 2026-10-07
Status: Accepted for the M1 harness; the distribution route is revisited in M4

## Context

The future target is Dolphin from Flathub (`org.kde.dolphin`). Its manifest at
commit `2aef43c` (2026-07-31) uses org.kde.Platform/Sdk 6.10, bundles
kde-cli-tools, baloo, konsole, dolphin, dolphin-plugins, libmtp, libssh, samba,
kio-extras (without configuration options) and ark as modules, and declares no
`add-extensions` (`flathub-dolphin/org.kde.dolphin.json:3-561`). Its sandbox
grants `home`, `/media`, `/run/media` and `~/.var/app` (`:8-27`); `home` alone
would not include `/run/media` (flatpak `common/flatpak-context.c:3862`). A
third-party worker cannot be added to the published app today.

In the runtime, `QT_PLUGIN_PATH` is `/app/lib/plugins:/usr/share/runtime/lib/plugins`
(runtime metadata), KDE apps install workers to `/app/lib/plugins/kf6/kio`
(observed in an installed KDE Flatpak), and the runtime ships kio-extras'
`archive.so` itself. Plugin ids are file base names
(`kcoreaddons/src/lib/plugin/kpluginmetadata.cpp:168-178`), so the app's copy
shadows the runtime's.

Extensions are mounted below a directory the app declares with `add-extensions`
(keys `directory`, `subdirectories`, `merge-dirs`, `add-ld-path`,
`version(s)`, flatpak `doc/flatpak-metadata.xml`) and are built with
`build-extension: true`.

flatpak-builder inside rootless podman/toolbx is reported to fail because
bubblewrap cannot create namespaces (containers/toolbox issue #863, Flathub forum
threads); no primary source says it works. Flathub recommends the
`org.flatpak.Builder` Flatpak.

kio-unpack's runtime dependencies are libarchive, zlib, liblzma, libbz2 and
libzstd from the runtime and, from M2 on, libudfread, which is not in the
runtime and is built from a pinned, patched commit (ADR 0003).

## Decision

**M1 harness.** A local development manifest, `flatpak/org.kde.dolphin.json`,
derived from the upstream file with two changes: branch `kio-unpack-dev` and the
module `kio-unpack` appended. In M1 nothing else is added, because M1 has no
UDF backend; M2 inserts `flatpak/modules/libudfread.json` before `kio-unpack`
together with the UDF backend (ADR 0003). It builds on the host
with `flatpak run org.flatpak.Builder`, not inside a rootless podman or toolbx
container; only user-level Flatpaks (Builder, Sdk) are added to the host. Every
dependency we add builds without network access in the build sandbox. Network
use is confined to one step: `flatpak-builder-lint` first rejects unpinned git
sources, `--download-only` then fetches every pinned source into the state
directory, and the build runs with `--disable-download`, which does no network
i/o and fails on a missing source (flatpak-builder(1); `flatpak/README.md`).

**Distribution route.**

| | (a) Upstream into kio-extras or as a KDE project | (b) Module in flathub/org.kde.dolphin | (c) Extension point in the Dolphin manifest + kio-unpack extension |
|---|---|---|---|
| Work for us | KDE review and release process | one PR per update | one-time manifest proposal, then our own extension repository |
| Licensing | GPL-2.0-or-later fits KDE policy | fine | fine |
| Release cadence | KDE Gear, every four months; the Flathub manifest already lags (26.04.3 vs 26.08 on Fedora) | tied to Dolphin manifest updates | independent |
| Coupling to runtime | built with Dolphin | built with Dolphin | extension branch must follow the runtime version (6.10 now) |
| Benefit beyond us | wide | none | any third-party KIO worker |
| Unknowns | acceptance, and our patched libudfread | maintainers' willingness | whether an app `--env=QT_PLUGIN_PATH` overrides the runtime value (OQ-10, spike S-5 in M1); Flathub acceptance |

Recommendation for M4: propose (c) to the KDE Flatpak maintainers with the
concrete manifest change (`add-extensions` with `directory: extensions/kio`,
`subdirectories: true`, `merge-dirs: lib/plugins/kf6/kio`, `add-ld-path: lib`,
and a matching `QT_PLUGIN_PATH`), demonstrated with the M1 harness; keep (a) as a
fallback, (b) as a last resort.

## Consequences

Flatpak breakage (hardcoded paths, missing SDK pieces, sandbox behaviour) shows
up from M1 on. The first M1 build compiles the whole Dolphin manifest; later
builds reuse flatpak-builder's module cache. The derived manifest must be
refreshed when upstream bumps the runtime. Users of the published Flathub
Dolphin get nothing until one of the routes succeeds.

## Alternatives considered

- **flatpak-builder inside a rootless podman or toolbx container:** rejected on
  the reported bwrap failures; can be retried if a primary source shows it works.
- **flatpak-builder RPM on the host:** needs a system-wide install, while the
  Flatpak route needs only user-level installs.
- **Host binaries from the sandbox (`flatpak-spawn --host`):** forbidden by the
  project constraints and by Flathub practice.
