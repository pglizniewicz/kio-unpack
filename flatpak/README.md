<!--
SPDX-License-Identifier: GPL-2.0-or-later
SPDX-FileCopyrightText: 2026 kio-unpack contributors
-->

# Local Flatpak dev build (M1)

This directory holds a **development** manifest that builds Dolphin from Flathub's
recipe with kio-unpack appended as the last module. Its only purpose is to catch
Flatpak breakage early (ADR 0010). It is not a submission to Flathub.

## Provenance

| File | Origin |
|---|---|
| `org.kde.dolphin.json` | `https://github.com/flathub/org.kde.dolphin`, commit `2aef43c4895667e1310d7284d03f476205d596aa` (2026-07-31), runtime org.kde.Platform 6.10. Changes: added `"branch": "kio-unpack-dev"`, appended module `kio-unpack`. Regenerate by re-applying these two changes to a newer upstream file. |
| `run_dolphin.sh`, `icoutils-gcc15.patch` | same commit, unchanged (referenced by the upstream modules) |
| `modules/libudfread.json` | written for this project; not yet referenced by the manifest (M2) |

The upstream manifest repository carries no LICENSE file. Treat the copied files
as upstream's and keep the commit reference above current.

The separate branch `kio-unpack-dev` keeps this build from replacing a Flathub
install of `org.kde.dolphin` (branch `stable`).

## Where builds run

flatpak-builder runs **on the host**, through the `org.flatpak.Builder` Flatpak,
never inside a rootless podman or toolbx container. Reports of bubblewrap failing
to create namespaces inside rootless podman (containers/toolbox issue #863,
Flathub forum threads) and the absence of any primary source saying it works led
to this choice.
Only user-level Flatpaks are added to the host.

## One-time host setup

```sh
flatpak install --user flathub org.flatpak.Builder org.kde.Sdk//6.10 org.kde.Platform//6.10
```

Check that the SDK carries libarchive development files. The upstream Ark module
in this manifest requires `LibArchive 3.3.3` and the manifest builds no
libarchive module, which strongly suggests the SDK has them; this check confirms it:

```sh
flatpak run --command=ls org.kde.Sdk//6.10 /usr/include/archive.h /usr/lib/x86_64-linux-gnu/pkgconfig/libarchive.pc
```

## Build, install, run (from the repository root)

Sources are fetched in a separate, controlled step, and the build itself runs
with `--disable-download`, which per flatpak-builder(1) does no network i/o and
fails if any source is not already local (`doc/flatpak-builder.xml`,
`--download-only` and `--disable-download`, flatpak-builder `bd35975d`).

1. Check that every source is pinned. The lint reports a git source without a
   commit, a tag without a commit, or a branch name instead of a commit hash as
   errors (`flatpak_builder_lint/checks/modules.py:68-94`, flatpak-builder-lint
   `d08b2299`); any `module-*-source-git-*` error blocks the next steps:
   ```sh
   flatpak run --command=flatpak-builder-lint org.flatpak.Builder manifest flatpak/org.kde.dolphin.json
   ```
2. Download every source into the state directory `.flatpak-builder/`
   (`downloads/<sha256>/` for archives and files, mirrored clones under `git/`).
   This is the only step that uses the network:
   ```sh
   flatpak run org.flatpak.Builder --download-only --state-dir=.flatpak-builder flatpak/build flatpak/org.kde.dolphin.json
   ```
3. Build and install without network access:
   ```sh
   flatpak run org.flatpak.Builder --user --install --force-clean --ccache --disable-download --state-dir=.flatpak-builder --repo=flatpak/repo flatpak/build flatpak/org.kde.dolphin.json
   ```

A missing source makes step 3 fail instead of downloading it, so a manifest
change that adds a source always goes through steps 1 and 2. To keep sources
across a deleted state directory, keep a mirror elsewhere and pass it with
`--extra-sources=<dir>` in step 2; it must use the state-directory layout, and
git sources in it must be mirrored clones (flatpak-builder(1),
`--extra-sources`).

The first build compiles every module of the Dolphin manifest (samba,
ghostscript, baloo, ark, ...) and takes a long time. Later builds reuse the module
cache in `.flatpak-builder/` and rebuild only modules whose sources changed,
normally just `kio-unpack`.

Interactive shell inside the build sandbox of our module:

```sh
flatpak run org.flatpak.Builder --build-shell=kio-unpack flatpak/build flatpak/org.kde.dolphin.json
```

Verify the worker inside the app sandbox:

```sh
flatpak run --branch=kio-unpack-dev --command=sh org.kde.dolphin -c 'ls /app/lib/plugins/kf6/kio; echo $QT_PLUGIN_PATH'
flatpak run --branch=kio-unpack-dev --command=kioclient org.kde.dolphin --platform offscreen ls unpack:$HOME/path/to/disc.iso
flatpak run --branch=kio-unpack-dev org.kde.dolphin
```

For double-click on a local ISO to open it as a folder, Dolphin's "Browse
compressed files as folders" setting (General > Behavior) must be enabled inside
the Flatpak as well; the sandboxed app keeps its own config in
`~/.var/app/org.kde.dolphin/config/dolphinrc`. The context-menu action and typed
`unpack:` URLs work without it.

The lint (step 1) has not been run yet. Besides the source-pinning errors it
may report issues that only matter for Flathub submission (such as the local
`dir` source); those do not block the dev build.

Remove the dev build:

```sh
flatpak uninstall --user org.kde.dolphin//kio-unpack-dev
```

Check C8 of spike S-4 (`docs/roadmap.md`, M1 step 4) installs the prototype
worker `kio_vproto` from a manifest copy with `"branch": "kio-unpack-proto"`,
so it never enters the `kio-unpack-dev` branch. Remove that branch once C8 is
recorded:

```sh
flatpak uninstall --user org.kde.dolphin//kio-unpack-proto
```

## M2 modules

`modules/` holds pinned module definitions, inserted before `kio-unpack` when
the corresponding backend lands:

- `libudfread.json` builds libudfread from VideoLAN's repository at commit
  `b0bc695` (2026-08-30), which contains robustness fixes not yet released;
  libudfread is not part of org.kde.Platform 6.10. Our metadata patch
  (ADR 0003) is added as a `patch` source when it exists in M2.

## Distribution

This manifest is a test harness. The distribution options (upstreaming, a module
in Flathub's Dolphin manifest, or an extension point) are compared in
`docs/adr/0010-flatpak-distribution-path.md`.
