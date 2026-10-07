<!--
SPDX-License-Identifier: GPL-2.0-or-later
SPDX-FileCopyrightText: 2026 kio-unpack contributors
-->

# AGENTS.md

Instructions for coding agents working in this repository. Human-facing
documentation lives in `docs/`. Before changing behaviour, read only what the
current stage needs:

1. the stage in `docs/roadmap.md`: scope, requirements, acceptance;
2. those requirements in `docs/requirements.md`;
3. from `docs/architecture.md`, only the sections that the box "Current stage"
   at its top lists;
4. the ADRs cited in those requirements and sections.

Do not implement anything else, even when it sits in the same file or
section. Sections that describe the finished v1 mark which parts belong to
which stage (architecture section 6, for example).

If `AGENTS.local.md` exists in the repository root, read it too and follow it.
It holds rules for the local machine (shell, build environment) and is not
committed.

## Project in one paragraph

`kio-unpack` is a read-only KF6/Qt6 KIO worker with scheme `unpack:` that lets
Dolphin browse disc images and unusual archives as folders. License:
GPL-2.0-or-later. Build system: CMake + extra-cmake-modules. Backends: libarchive
(M0), libudfread for UDF (M2), own optical-image parsers for CUE/BIN, MDS/MDF,
CCD and TOC (M3); 7-Zip and libmirage are not used (ADR 0002, ADR 0004).
Status and milestones:
`docs/roadmap.md`.

## Hard constraints

- **Develop from the build tree, never from an installation.** Build out of tree
  in `build/` (or `build-*/`); tests, `kioclient` and Dolphin load the worker
  through `QT_PLUGIN_PATH=$PWD/build/bin` (ADR 0009). Building, testing and the
  dev loop must not need root or an install step; never `cmake --install` into
  a system prefix during development.
- **Dependencies are listed in `docs/building.md`.** A new dependency goes into
  that table (with its Fedora package name) and into `scripts/toolbox-setup.sh`,
  which installs the same list on Fedora; keep the two in sync. Prefer
  distribution packages; anything that must be vendored needs an ADR and a
  matching module in `flatpak/modules/`.
- **No hardcoded `/usr` paths in code or CMake.** Derive locations from
  `KDEInstallDirs6` / `GNUInstallDirs` variables or configuration.
  The code must build unchanged under flatpak-builder with prefix `/app`.
- **Every dependency must be buildable from source as a flatpak-builder module
  against org.kde.Sdk**, with no network access in the build sandbox.
- **No host binaries at runtime** (no `flatpak-spawn --host`, no
  `QProcess` on host tools). CLI-wrapping backends (unar, deark, Aaru) are
  optional, live outside `src/core`, and must degrade gracefully when absent.
- **v1 is read-only**: `listDir`, `stat`, `get`. No writing into archives, no
  sudo, no loop mounts, no FUSE.
- Access errors (sandbox, permissions, missing files) must become KIO errors,
  never crashes or empty listings.
- Commands in documentation must not depend on a particular interactive shell:
  write them so they run unchanged in bash, zsh and fish. No heredocs, no
  command substitution in any form (neither POSIX `$(...)` nor fish's
  `(...)`); use `$PWD` instead of `$(pwd)`, and put anything that needs a
  computed value into a `bash` script under `scripts/`. No `VAR=x cmd` (use
  `env VAR=x cmd`).

## Commands

Run from the repository root, in an environment that has the dependencies from
`docs/building.md`. When you build in a container, run each command inside it
(for toolbx: `toolbox run -c <container> <command>`).

The block lists the commands of the finished project. Each comment names the
stage that creates what the command needs (`docs/roadmap.md`); until that
stage is done, the script, CMake option or fixture does not exist. Do not run
such a command earlier, and do not create its script or option outside the
work of its stage.

This block is the only place where project commands are written out;
`docs/building.md` and `docs/roadmap.md` refer to its comment lines. Change a
command here, not in a copy. The Flatpak commands are the exception and live
in `flatpak/README.md` and the M1 steps.

```sh
# Optional, Fedora with toolbx, run on the host (idempotent): create and
# provision a container `kio-unpack` with every dependency
bash scripts/toolbox-setup.sh

# [M2] Build vendored dependencies (pinned, patched libudfread; ADR 0003)
bash scripts/build-deps.sh

# [M0a] Configure, build, test
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure

# [M0a] Prototype build for spikes (prototypes/, deleted once each spike is
# recorded; docs/roadmap.md, "Spikes before dependent work")
cmake -S . -B build-proto -G Ninja -DCMAKE_BUILD_TYPE=Debug -DKIO_UNPACK_PROTOTYPES=ON
cmake --build build-proto

# [M0c] Sanitizer build (separate build dir)
cmake -S . -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON '-DECM_ENABLE_SANITIZERS=address;undefined'
cmake --build build-asan
ctest --test-dir build-asan --output-on-failure

# [M0c] Fuzz build (clang + libFuzzer)
env CC=clang CXX=clang++ cmake -S . -B build-fuzz -G Ninja -DKIO_UNPACK_FUZZ=ON
cmake --build build-fuzz

# [M0a] Smoke test the worker from the build tree, without a GUI
env QT_PLUGIN_PATH=$PWD/build/bin kioclient --platform offscreen ls unpack:$PWD/build/tests/fixtures/rr.iso
env QT_PLUGIN_PATH=$PWD/build/bin kioclient --platform offscreen cat unpack:$PWD/build/tests/fixtures/rr.iso/dir/file.txt

# [M0a] Check that KIO discovers the worker
env QT_PLUGIN_PATH=$PWD/build/bin kioclient --platform offscreen stat unpack:$PWD/build/tests/fixtures/rr.iso

# [M0a] Dev loop: a new Dolphin window that loads the worker from the build
# tree. --new-window is mandatory, otherwise the URL is handed to a running
# Dolphin. XDG_CONFIG_HOME keeps the user's dolphinrc and kio_unpackrc
# untouched; the first command enables "Browse compressed files as folders"
# in that separate directory (ADR 0009).
env XDG_CONFIG_HOME=$PWD/build/dev-config kwriteconfig6 --file dolphinrc --group General --key BrowseThroughArchives true
env XDG_CONFIG_HOME=$PWD/build/dev-config QT_PLUGIN_PATH=$PWD/build/bin dolphin --new-window $PWD/build/tests/fixtures

# [M0a] Debug a worker: make it stop at startup, then attach gdb
env KIOWORKER_DEBUG_WAIT=unpack QT_PLUGIN_PATH=$PWD/build/bin kioclient --platform offscreen ls unpack:/path/to/disc.iso

# [M0c] Lint everything: formatting, clang-tidy, cppcheck, REUSE, ShellCheck,
# codespell, markdownlint (needs a configured build/ for clang-tidy, ADR 0014)
bash scripts/lint.sh
# Apply KDE formatting (clang-format) in place
bash scripts/lint.sh --fix

# [M2] Stress tests (large real fixtures; also run weekly in CI)
cmake -S . -B build-stress -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTING=ON -DKIO_UNPACK_STRESS_TESTS=ON
cmake --build build-stress
ctest --test-dir build-stress -L stress --output-on-failure

# [M0c] Clazy build (clang with the clazy plugin; warnings are errors)
env CC=clang CXX=clang++ cmake -S . -B build-clazy -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DENABLE_CLAZY=ON
cmake --build build-clazy

# [M0c] Coverage report (HTML in build-coverage/coverage.html)
cmake -S . -B build-coverage -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DBUILD_COVERAGE=ON
cmake --build build-coverage
ctest --test-dir build-coverage --output-on-failure
gcovr --root . --filter src/ --html-details build-coverage/coverage.html build-coverage

# [M0c] Optional, toolbx users: pre-commit hook that runs scripts/lint.sh --staged and
# re-runs itself in the named container when the tools are not on the host
ln -s ../../scripts/pre-commit.sh .git/hooks/pre-commit
git config kio-unpack.toolbox kio-unpack
```

Flatpak builds (M1) run on the host via `org.flatpak.Builder`, not inside a
rootless podman or toolbx container; see `flatpak/README.md`.

## Layout

```
src/core/          libkio-unpack-core (Qt Core + KI18n, no KIO): FileByteSource, Sniff,
                   EntryTable, PathResolver (M0a); CachedByteSource, SessionCache (M0b);
                   BufferByteSource (M0c); BackendRegistry with priorities and fallback,
                   other ByteSources, data locators (M2, M3)
src/backends/<n>/  one directory per backend: libarchive (M0a ISO, M0b the rest),
                   udf, partition, eltorito (M2), optical (M3)
src/worker/        kio_unpack plugin: kdemain, UnpackWorker, unpack.json
src/cli/           kio-unpack-cli (list|stat|cat) for smoke and corpus runs
src/action/        kio-unpack-action + Dolphin service menus for local files and unpack: items (M2, ADR 0005, ADR 0007)
tests/             QtTest unit + worker integration tests, fixture generators
fuzz/              libFuzzer harnesses (M0c; built with -DKIO_UNPACK_FUZZ=ON)
prototypes/        throwaway checks of KIO/Dolphin behaviour (built with -DKIO_UNPACK_PROTOTYPES=ON),
                   deleted once decided; results stay in docs/prototypes/
scripts/           toolbox-setup.sh (optional, Fedora); extract-messages.sh (M0b), lint.sh and pre-commit.sh (M0c); build-deps.sh (M2); corpus-fetch.sh and corpus-smoke.sh (M2)
.github/workflows/ CI on GitHub Actions in a fedora:44 container (build + ctest from M0a, all jobs from M0c; ADR 0014)
po/                kio6_unpack.pot and per-language catalogues (M0b)
flatpak/           local dev manifest derived from flathub/org.kde.dolphin
docs/              architecture, requirements (EARS), ADRs, roadmap, prototype records
LICENSES/          GPL-2.0-or-later.txt (ADR 0012); REUSE.toml joins it in M0c
```

Create a component only in the stage named next to it above; do not stub later components ahead of time.
M0 is split into the stages M0a, M0b and M0c (`docs/roadmap.md`); work on one
stage at a time.

## Conventions

- Documentation, code, comments, commit messages: English.
- Every user-visible string goes through KI18n (`i18n`, `i18nc`, `ki18n`) with
  `TRANSLATION_DOMAIN=kio6_unpack`; after changing strings run
  `bash scripts/extract-messages.sh` (from M0b) and update
  `po/pl/kio6_unpack.po` (ADR 0013). `.desktop` files carry `Name[pl]=` inline.
- C++20, KDE Frameworks coding style; include ECM `KDECompilerSettings`
  (`KDE_COMPILERSETTINGS_LEVEL` 6.13), `KDEInstallDirs6`, `KDECMakeSettings`.
  Format with KDE's clang-format style through `kde_clang_format`;
  `.clang-format` is generated by CMake and not committed (ADR 0014).
- Warnings are errors (`KIO_UNPACK_WERROR`, default `ON`); only the Flatpak
  manifest turns it off. Fix the warning instead of disabling it.
- Before declaring work done, `bash scripts/lint.sh` and the clazy build must
  pass (from M0c, when they exist; before that, the build with warnings as
  errors and `ctest`). Suppress a finding only inline and with a reason
  (`// NOLINT(<check>): <reason>`, `// cppcheck-suppress <id>`,
  `// clazy:exclude=<check>`); project-wide exclusions go into the tool's
  configuration file with a comment (ADR 0014).
- Every source file starts with SPDX headers:
  `SPDX-License-Identifier: GPL-2.0-or-later` and `SPDX-FileCopyrightText`.
- The worker plugin target is `kio_unpack`, built with
  `kcoreaddons_add_plugin(kio_unpack INSTALL_NAMESPACE "kf6/kio")`; it lands in
  `build/bin/kf6/kio/kio_unpack.so`.
- The binding interface contracts are the three listed at the start of
  section 4 of `docs/architecture.md`; the code sketches there show one possible
  shape, not signatures to copy. Numeric defaults (cache sizes, caps, timeouts)
  are starting values listed in section 9.1, to be measured: keep each in one
  place (a configuration key or one named constant), never repeat it as a
  literal, and do not let tests depend on the defaults.
- Errors: map every failure to the KIO error table in `docs/architecture.md`.
  Never let an exception escape a `WorkerBase` virtual (use `guard()`).
- Never call `abort()` or `alarm()` in the worker: KCrash would raise a crash
  dialog and KIO uses `SIGALRM` itself. The watchdog ends a hung operation with
  `_exit()`.
- Poll `wasKilled()` in every loop over entries or data chunks.
- From M0c on, every parser of our own that reads untrusted bytes (partitions,
  El Torito, data locators, checkpointing decoder, optical descriptors) lands
  in the same change as its libFuzzer harness (REQ-082, ADR 0011).
- Requirements: tests reference requirement IDs in their names or a comment,
  e.g. `// REQ-010`. A new externally observable behaviour needs a new `REQ-NNN`
  in `docs/requirements.md` and a test.
- Significant decisions get a new ADR in `docs/adr/NNNN-title.md` (prose,
  numbered sequentially). Until the first push to the public repository, records
  are edited in place; after it, an accepted record is never rewritten; supersede
  it instead (ADR 0001).
- Ordinary tests write at most 64 MiB of fixtures and decode at most 64 MiB;
  reach large sizes and offsets with `SparseByteSource`, and put real large
  data into stress tests (`docs/architecture.md`, section 13).
- Test fixtures are generated by the build (xorriso, mksquashfs, mkfs.vfat,
  zip) into `build/tests/fixtures`, so the smoke and dev-loop commands find
  them right after `cmake --build`. Commit a binary fixture only if it is smaller than 100 KB and cannot be
  generated. Never commit corpus samples; they go to the untracked `.cache/`.
- A decision that depends on how KIO, Dolphin or Flatpak behave at run time
  needs a spike before the dependent work: listed in `docs/roadmap.md`
  ("Spikes before dependent work") with question, timing, owner, the
  requirements it gates and a fallback; recorded in `docs/prototypes/`.
  Requirements it gates stay provisional until the record exists. Do not
  implement gated work before its spike is recorded.
- Mark anything taken from memory rather than a primary source as UNVERIFIED in
  docs.

## Git

- The repository is hosted on GitHub (ADR 0013).
- Do not commit, push, or create branches unless the human asks.
- Author identity comes from `git config user.email`; never use any other address.
