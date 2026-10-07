<!--
SPDX-License-Identifier: GPL-2.0-or-later
SPDX-FileCopyrightText: 2026 kio-unpack contributors
-->

# 0009. Development loop: run Dolphin and kioclient against the build tree

Date: 2026-10-07
Status: Accepted

## Context

A KIO worker is a plugin. During development it has to be loaded by Dolphin and
`kioclient` without being installed, and without tying contributors to one
distribution or one way of setting up their machine.

- KIO launches workers out of process with the absolute plugin path found
  through `KPluginMetaData::findPlugins("kf6/kio")`
  (`kio/src/core/worker.cpp:438-531`). That search looks in the application
  directory, then every `QT_PLUGIN_PATH` entry, then Qt's plugin directory, and
  keeps the first plugin per id (`kcoreaddons/src/lib/plugin/kpluginmetadata.cpp:56-95,253-300`;
  `qtbase/src/corelib/kernel/qcoreapplication.cpp:3032-3040`). A worker in the
  build tree is found without installing it; KIO's own tests rely on this
  (`kio/autotests/http/CMakeLists.txt:107-114`).
- Plugins land in `build/bin/kf6/kio/` (ECM `KDECMakeSettings.cmake:271`,
  `kcoreaddons/KF6CoreAddonsMacros.cmake:64`).
- Dolphin hands its URLs to a running instance over D-Bus and exits unless
  started with `--new-window` (`dolphin/src/main.cpp:194-207`).
- `kioclient` (kde-cli-tools) offers `ls`, `cat`, `stat` and `copy`, and runs
  without a GUI with `--platform offscreen`.
- The plugin is linked against the KF6 and Qt of the environment it was built
  in. On the maintainer's machine that environment is a toolbx container
  (Fedora 44) on a Fedora 44 host with KDE Plasma 6 on Wayland; the container
  shares the session bus, the Wayland socket and the home directory with the
  host, and on 2026-10-07 both had KF 6.30, Qt 6.11.2 and Dolphin 26.08.1.

## Decision

Development builds are not installed. Dolphin and `kioclient` load the worker
from the build tree through `QT_PLUGIN_PATH`, and they come from the same
environment that built it: the host's own Dolphin when building on the host,
the container's Dolphin when building in a container. The commands are the
block "[M0a] Dev loop" in `AGENTS.md`, the only copy.

`--new-window` is mandatory. `XDG_CONFIG_HOME` gives this Dolphin and its
workers a configuration directory of their own, in which the first command
enables "Browse compressed files as folders" (off by default); the user's
`dolphinrc` and `kio_unpackrc` are never changed by manual tests. The inner loop is `kioclient --platform offscreen`
in the same environment; the automated loop is `ctest`. Workers are debugged
with `KIOWORKER_DEBUG_WAIT=unpack` (`kio/src/kioworker/kioworker.cpp:76,116-129`)
and gdb, again in the environment that built them.

## Consequences

Plugin, KF6, Qt and all libraries come from one environment, so ABI drift cannot
cause false failures, and our own builds of dependencies are found the same way
the build found them. Nothing is installed into a system prefix, and no
particular distribution, container tool or shell is required. A Dolphin
started without its own `XDG_CONFIG_HOME` would read and write the user's
`~/.config/dolphinrc`, which a container that shares the home directory also
shares with the host's Dolphin; with it, the dev Dolphin starts with default
settings, theme and file associations, which manual checks accept.

## Alternatives considered

- **Building in a container and running the host's Dolphin with
  `QT_PLUGIN_PATH` at the build tree:** works while the two environments have
  the same KF6 and Qt, breaks silently when one side updates first.
- **Installing into a prefix:** the same ABI risk when the prefix is used from a
  different environment, plus an install step; a system prefix also needs root.
- **Testing only through the Flatpak:** far slower; kept as M1's separate check.
