<!--
SPDX-License-Identifier: GPL-2.0-or-later
SPDX-FileCopyrightText: 2026 kio-unpack contributors
-->

# 0014. Code conventions and static analysis

Date: 2026-10-07
Status: Accepted

## Context

The code should look and be checked like KDE code. extra-cmake-modules 6.30
(Fedora 44) provides the pieces:

- `KDEClangFormat` adds `kde_clang_format(<files>)` and a `clang-format` target.
  It copies ECM's `.clang-format` into the source directory at configure time,
  overwrites it while it still carries ECM's marker, and recommends not putting
  it under version control (`/.clang-format` in `.gitignore`)
  (`kde-modules/KDEClangFormat.cmake`, module documentation).
- `KDEGitCommitHooks` adds `kde_configure_git_pre_commit_hook(CHECKS
  CLANG_FORMAT JSON_SCHEMA)`; the format check runs `git clang-format`, the JSON
  check `check-jsonschema`.
- `KDECompilerSettings` with `KDE_COMPILERSETTINGS_LEVEL` 6.13 sets C++20,
  `-Wall -Wextra -pedantic`, `-Werror=undef` and Qt definitions such as
  `QT_NO_CAST_FROM_ASCII`, `QT_NO_KEYWORDS`, `QT_NO_FOREACH` and
  `QT_STRICT_ITERATORS` (`kde-modules/KDECompilerSettings.cmake:39-110`).
- `ECMDeprecationSettings` adds `ecm_set_disabled_deprecation_versions(QT …
  KF …)`.
- `KDECMakeSettings` adds the option `ENABLE_CLAZY`, which loads the clazy
  plugin into clang (`kde-modules/KDECMakeSettings.cmake:306-315`).
- `ECMCoverageOption` adds `BUILD_COVERAGE` (gcov instrumentation).
- No ECM module enables `CMAKE_EXPORT_COMPILE_COMMANDS`.

KDE's CI templates (`sysadmin/ci-utilities` `ac47e191`, `gitlab-templates/`)
check formatting with ECM's `.clang-format` (`clang-format.yml`), run cppcheck
with `--library=qt --library=kde --check-level=exhaustive --inline-suppr`
(`run-cppcheck.py:82-87`), run `reuse lint` (`reuse-lint.yml`), validate JSON
against the KPluginMetaData schema (`json-validation.yml`) and can run
pre-commit (`pre-commit.yml`). Clazy appears nowhere in ci-utilities; KDE
develops it (`sdk/clazy`), and its default level is `level1`, which includes
`level0` (`clazy/README.md:212-220`, `6dcee86c`). KDE CI does not run
clang-tidy.

Fedora 44 packages every tool needed: `clang-tools-extra` 22 (clang-format,
clang-tidy, `run-clang-tidy`), `clazy` 1.17, `cppcheck` 2.22, `reuse` 6.2,
`ShellCheck` 0.11, `codespell` 2.4, `markdownlint-cli2` 0.23 and `gcovr` 8.6.
`check-jsonschema` is not packaged in Fedora 44, `git-clang-format` is packaged
but not installed in the container, and the host has only git: it must stay clean (no `sudo dnf`), while builds
and tools run in the toolbx container.

This project parses untrusted data, so checks that find memory and logic bugs
weigh more here than in a typical KDE application.

## Decision

1. **Formatting:** KDE's style from ECM. `kde_clang_format()` covers `src/`,
   `tests/` and `fuzz/`; `.clang-format` is generated at configure time and is
   listed in `.gitignore`, as ECM recommends. Throwaway spike code in
   `prototypes/` is left out of formatting, `scripts/lint.sh` and the clazy
   build, because it is deleted once its spike is recorded; it still builds
   with warnings as errors.
2. **Compiler settings:** `KDE_COMPILERSETTINGS_LEVEL` 6.13, the Qt definitions
   it brings, `ecm_set_disabled_deprecation_versions(QT <min> KF <min>)` with
   the minimum versions from `find_package`, and
   `CMAKE_EXPORT_COMPILE_COMMANDS ON` for clang-tidy, clazy and editors.
3. **`-Werror`:** the CMake option `KIO_UNPACK_WERROR`, default `ON`, makes
   warnings errors in the native, sanitizer, fuzz and clazy builds and in CI.
   The Flatpak manifest sets it `OFF`, because the compiler in `org.kde.Sdk`
   changes with SDK updates that we do not control.
4. **Analysers:**
   - **clang-tidy** with a `.clang-tidy` at the root enabling `bugprone-*`,
     `clang-analyzer-*`, `performance-*` and selected `cert-*` checks, all as
     errors, run by `run-clang-tidy` over `build/compile_commands.json`.
     Options that only GCC knows are ignored with
     `--extra-arg=-Wno-unknown-warning-option`; the exact list of checks is
     settled in M0c, when the first code exists.
   - **cppcheck** with KDE CI's arguments plus `--error-exitcode=1`.
   - **clazy** at its default level (`level1`) in a separate clang build
     `build-clazy` with `ENABLE_CLAZY=ON`; with `-Werror` its warnings fail the
     build.
5. **Other checks:** `reuse lint` (SPDX headers and `LICENSES/`, with
   `REUSE.toml` for files that cannot carry headers, such as JSON and `.po`),
   ShellCheck on `scripts/`, codespell on code and `docs/`, markdownlint-cli2 on
   `docs/` and the top-level Markdown files, `msgfmt --check` on catalogues.
   KDE CI's validation of `unpack.json` against the KPluginMetaData schema is
   left out, because `check-jsonschema` is not packaged in Fedora 44; the
   integration tests load the plugin through KIO and fail on broken metadata.
6. **Coverage:** `ECMCoverageOption` in a separate build `build-coverage`;
   gcovr writes an HTML report. There is no threshold that fails a build.
7. **One entry point:** `scripts/lint.sh` runs formatting check, clang-tidy,
   cppcheck, `reuse lint`, ShellCheck, codespell and markdownlint-cli2 and exits
   non-zero on any finding. `--fix` applies clang-format; `--staged` limits the
   file-based checks to files staged in git and skips clang-tidy and cppcheck,
   which need a configured build. The clazy and coverage builds are separate
   commands in `AGENTS.md`.
8. **Pre-commit hook:** `scripts/pre-commit.sh`, linked as
   `.git/hooks/pre-commit`, runs `scripts/lint.sh --staged`. If clang-format is
   not on `PATH` and `git config kio-unpack.toolbox` names a container, the
   script re-runs itself with `toolbox run -c <container>`. The host needs
   nothing besides git and toolbox, and the hook uses the container's tools.
   Machine-specific setup belongs in `AGENTS.local.md`.
9. **CI:** GitHub Actions in a `fedora:44` container. Packages come from
   `bash scripts/toolbox-setup.sh --ci --print-packages`, so CI, the toolbx
   container and `docs/building.md` share one list. The script has three
   profiles: core (always), m3 (optical-image fixture tools that the M3 tests
   need) and optional (reference tools no test needs); `--ci` drops the
   graphical dev-loop packages (Dolphin, the Wayland platform plugin, the icon
   theme), which no test needs, and keeps `kioclient` for the smoke tests. CI
   installs core until M3 and core + m3 (`--ci --m3 --print-packages`) from M3
   on; it never installs optional. Every job installs the same list, also
   M0a's single build-and-test job, which does not use the lint tools yet:
   one list is simpler than a list per job. Jobs: GCC build and tests,
   sanitizer build and tests, clazy build, fuzz build with a 60-second run
   of every harness (REQ-082), `scripts/lint.sh`, and a coverage report kept
   as an artifact. A separate
   workflow runs weekly and on demand (`schedule`, `workflow_dispatch`) with
   `-DKIO_UNPACK_STRESS_TESTS=ON` and `ctest -L stress`, for tests too large
   for every push (from M2: the 512 MiB gzip read, the 4.5 GiB UDF file).
10. **Suppressions:** only inline and with a reason:
    `// NOLINT(<check>): <reason>`, `// cppcheck-suppress <id>` with a comment,
    `// clazy:exclude=<check>`. A check is disabled for the whole project only
    in its configuration file, with a comment saying why.
11. **Rolling environment:** the toolbx container and CI both use Fedora 44 with
    its current updates; nothing is pinned, and their tool versions drift over
    time and can differ from each other between two upgrades. Every CI job saves
    the installed package versions (`rpm -qa --qf
    '%{NAME}-%{VERSION}-%{RELEASE}.%{ARCH}\n' | sort`) as the artifact
    `rpm-versions.txt`. `scripts/lint.sh` prints the versions of the compiler
    and every tool it runs before running them, and `scripts/toolbox-setup.sh`
    reports them after provisioning. When a local result and CI disagree,
    the first step is to compare these versions and upgrade the container with
    `bash scripts/toolbox-setup.sh`.

## Consequences

Code follows KDE's style and warning set, and the checks KDE CI runs pass
before code reaches the repository. Because the environment is rolling, a
Fedora update can reach CI before the container is upgraded, or the other way
round: for a while the hook and CI can then disagree on formatting, or a new
GCC, clang, Qt or KF can add warnings that stop one build and not the other.
`rpm-versions.txt` and the versions printed by `scripts/lint.sh` make such a
difference visible; upgrading the container resolves it. This is accepted in
exchange for security fixes in the format libraries and a warning-free tree. The Flatpak build is not stopped by such warnings. The hook adds the start
time of a toolbx container to each commit from the host. Editors see
`.clang-format` only after the first CMake configure.

## Alternatives considered

- **Pinned environment** (container image by digest plus a frozen package
  repository): reproducible, but Fedora's update repository keeps only the
  newest build of each package (UNVERIFIED; older builds stay in Koji, not in an
  installable repository), so pinning needs a mirror we would maintain, and
  pinned format libraries miss security fixes.
- **ECM's `KDEGitCommitHooks`:** needs `git clang-format` where git runs; on
  the host that means `sudo dnf install clang-tools-extra git-clang-format`,
  about 330 MB with `clang-libs` and `llvm-libs`, against the clean-host rule,
  and a clang-format version kept in step with the container by hand.
- **The pre-commit framework:** KDE CI supports it, but it adds a tool whose
  only job here would be calling the same commands that `scripts/lint.sh`
  calls.
- **Committing `.clang-format`:** ECM recommends against it and regenerates the
  file while it carries ECM's marker.
- **`-Werror` only in CI:** lets warnings pile up locally until CI fails.
- **`-Werror` in the Flatpak too:** an SDK update could stop the M1 build with
  no change in our code.
- **No clang-tidy, as in KDE CI:** clang-tidy's bugprone and analyser checks
  find exactly the bugs that matter in parsers of untrusted data.
