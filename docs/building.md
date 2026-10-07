<!--
SPDX-License-Identifier: GPL-2.0-or-later
SPDX-FileCopyrightText: 2026 kio-unpack contributors
-->

# Building kio-unpack

kio-unpack builds with CMake and extra-cmake-modules on any Linux distribution
that ships KDE Frameworks 6 and Qt 6. Nothing has to be installed for
development: tests, `kioclient` and Dolphin load the worker straight from the
build tree (ADR 0009).

## Dependencies

This table is the reference list. `scripts/toolbox-setup.sh` installs the same
set on Fedora; other distributions use their own package names. The script
groups the rows into three profiles: core (every row without a milestone or
"Optional" mark and the M2 row), m3 (`--m3`: the "Test fixtures (M3)" row)
and optional (`--optional`: the "Optional" row); `--all` selects all three.
`--ci` leaves out the "Dev loop (GUI)" row, which no test needs; CI installs
that list (ADR 0014).

| Purpose | Component | Fedora package |
|---|---|---|
| Build | C++20 compiler (GCC or Clang) | `gcc-c++` |
| Build | CMake, Ninja | `cmake`, `ninja-build` |
| Build | git | `git` |
| Build | extra-cmake-modules | `extra-cmake-modules` |
| Build | Qt 6 Base | `qt6-qtbase-devel` |
| Build | KF6 KIO, KCoreAddons, KI18n, KConfig | `kf6-kio-devel`, `kf6-kcoreaddons-devel`, `kf6-ki18n-devel`, `kf6-kconfig-devel` |
| Build | gettext (`xgettext`, `msgfmt`) | `gettext` |
| Backends | libarchive | `libarchive-devel` |
| Backends | zlib, liblzma, libbz2, libzstd | `zlib-ng-compat-devel`, `xz-devel`, `bzip2-devel`, `libzstd-devel` |
| Backends (M2) | Meson, to build the pinned libudfread with `scripts/build-deps.sh` (ADR 0003) | `meson` |
| Dev loop, smoke tests | `kioclient`; `cmp` and `diff` (the smoke test compares output with its sources) | `kde-cli-tools`, `diffutils` |
| Dev loop (GUI) | Dolphin, Qt Wayland platform plugin, an icon theme (a desktop installation already has the last two), kio-extras (thumbnailers and the archive worker), thumbnailers for PDF and other documents (spike S-6), `kwriteconfig6` (KConfig's command-line tool; the dev loop uses it to write `dolphinrc`) | `dolphin`, `qt6-qtwayland`, `breeze-icon-theme`, `kio-extras`, `kdegraphics-thumbnailers`, `kf6-kconfig` (also pulled in by `kf6-kconfig-devel`) |
| Test fixtures | xorriso, genisoimage, squashfs-tools, udftools, dosfstools, mtools, zip, 7-Zip (`7z`, REQ-057), unzip, hfsplus-tools (`mkfs.hfsplus`) | same names; 7-Zip is `7zip` |
| QA | gdb, Clang with compiler-rt (libFuzzer), ASan and UBSan runtimes | `gdb`, `clang`, `compiler-rt`, `libasan`, `libubsan` |
| Lint (ADR 0014) | clang-format, clang-tidy, clazy, cppcheck, REUSE, ShellCheck, codespell, markdownlint-cli2 | `clang-tools-extra`, `clazy`, `cppcheck`, `reuse`, `ShellCheck`, `codespell`, `markdownlint-cli2` |
| Coverage (ADR 0014) | gcovr | `gcovr` |
| Test fixtures (M3) | vcdimager, FFmpeg | `vcdimager`, `ffmpeg-free` |
| Optional | unar, hfsutils (reference tools; never required by the core) | `unar`, `hfsutils` |

There is no handbook in 1.0, so KDocTools is not a dependency.

## Build and test

The commands to configure, build and test, and those for smoke tests, the dev
loop, sanitizers, fuzzing, lint, clazy and coverage, are kept in one place:
`AGENTS.md`, section "Commands", where each is tagged with the stage that
introduces it (`docs/roadmap.md`). Warnings are errors by default
(`KIO_UNPACK_WERROR=ON`, ADR 0014).

## Building in a container

The commands work unchanged inside a development container; run each of them
inside it. On Fedora, `bash scripts/toolbox-setup.sh` (run on the host) creates a
toolbx container `kio-unpack` with every package from the table, and
`toolbox run -c kio-unpack <command>` runs a command in it. When Dolphin runs in
the container, it must be started there too, so that it uses the same KF6 and
Qt as the build (ADR 0009).

Flatpak builds are the exception: flatpak-builder runs on the host, not inside a
rootless podman or toolbx container (ADR 0010, `flatpak/README.md`).
