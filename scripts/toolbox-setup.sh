#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: 2026 kio-unpack contributors
#
# Optional helper for Fedora: create and provision the `kio-unpack` toolbx
# container (Fedora 44) with every dependency from docs/building.md.
#
# Idempotent: creating an existing container is skipped, and `dnf install`
# of already installed packages is a no-op. Run it from the HOST:
#
#     bash scripts/toolbox-setup.sh              # profile core: build, test, lint
#     bash scripts/toolbox-setup.sh --m3         # core + m3 (optical-image fixtures)
#     bash scripts/toolbox-setup.sh --optional   # core + optional reference tools
#     bash scripts/toolbox-setup.sh --all        # core + m3 + optional
#     bash scripts/toolbox-setup.sh --m3 --print-packages   # print, do nothing
#     bash scripts/toolbox-setup.sh --ci --print-packages   # CI's list
#
# Profiles: core is always included; m3 holds tools the M3 tests need; optional
# holds reference tools that no test needs. --ci leaves out the graphical
# dev-loop packages (Dolphin, Wayland platform plugin, icon theme), which no
# test needs. --print-packages prints the selected list and exits; it runs
# anywhere and touches nothing. CI installs that list in a fedora container:
# --ci until M3, --ci --m3 from M3 on (ADR 0014).
#
# docs/building.md is the reference dependency list; this script installs the
# same set under Fedora package names. Keep the two in sync.
# Nothing here touches host system directories.

set -euo pipefail

CONTAINER="${KIO_UNPACK_CONTAINER:-kio-unpack}"
RELEASE="44"

# --- package groups -----------------------------------------------------------

# Toolchain and KDE build system.
PKGS_BUILD=(
    cmake
    ninja-build
    gcc-c++
    git
    extra-cmake-modules
    qt6-qtbase-devel
    kf6-kio-devel
    kf6-kcoreaddons-devel
    kf6-ki18n-devel
    kf6-kconfig-devel
    gettext
)

# Format libraries from Fedora (M0: libarchive; M2: decoders). libudfread is
# built by scripts/build-deps.sh from a pinned, patched commit (ADR 0003), so
# only its build tool is installed here.
PKGS_BACKENDS=(
    libarchive-devel
    meson
    zlib-ng-compat-devel
    xz-devel
    bzip2-devel
    libzstd-devel
)

# Dev loop and smoke tests: kioclient; cmp and diff for the smoke test,
# which a minimal fedora container (CI) lacks.
PKGS_DEVLOOP=(
    kde-cli-tools
    diffutils
)

# Graphical dev loop: Dolphin runs inside this container, on the host's
# Wayland socket. No test needs it, so --ci leaves it out. kio-extras and
# kdegraphics-thumbnailers bring the thumbnailers and the archive worker that
# spike S-6 and the M0b acceptance need. kf6-kconfig carries kwriteconfig6,
# which the dev loop uses to write dolphinrc (kf6-kconfig-devel pulls it in
# too; listed so the dev loop does not depend on that).
PKGS_GUI=(
    dolphin
    qt6-qtwayland
    breeze-icon-theme
    kio-extras
    kdegraphics-thumbnailers
    kf6-kconfig
)

# Tools that generate test fixtures at test time.
PKGS_FIXTURES=(
    xorriso
    genisoimage
    squashfs-tools
    udftools
    dosfstools
    mtools
    zip
    7zip
    unzip
    hfsplus-tools
)

# Debugging, sanitizers, fuzzing (libFuzzer ships with clang + compiler-rt).
PKGS_QA=(
    gdb
    clang
    compiler-rt
    libasan
    libubsan
)

# Formatters, linters, static analysers and coverage (ADR 0014).
PKGS_LINT=(
    clang-tools-extra
    clazy
    cppcheck
    reuse
    ShellCheck
    codespell
    markdownlint-cli2
    gcovr
)

# Profile m3: tools that generate optical-image fixtures (Video CD, audio
# tracks). Required by the M3 tests, so CI installs them from M3 on.
PKGS_M3=(
    vcdimager
    ffmpeg-free
)

# Profile optional: CLI backends and reference tools. No test requires them.
PKGS_OPTIONAL=(
    unar
    hfsutils
)

# --- argument handling --------------------------------------------------------

WITH_M3=0
WITH_OPTIONAL=0
WITH_GUI=1
PRINT_ONLY=0
for arg in "$@"; do
    case "$arg" in
        --m3) WITH_M3=1 ;;
        --optional) WITH_OPTIONAL=1 ;;
        --all) WITH_M3=1; WITH_OPTIONAL=1 ;;
        --ci) WITH_GUI=0 ;;
        --print-packages) PRINT_ONLY=1 ;;
        -h|--help)
            sed -n '2,28p' "$0"
            exit 0
            ;;
        *)
            echo "unknown argument: $arg" >&2
            exit 2
            ;;
    esac
done

# --- package list -------------------------------------------------------------

PKGS=(
    "${PKGS_BUILD[@]}"
    "${PKGS_BACKENDS[@]}"
    "${PKGS_DEVLOOP[@]}"
    "${PKGS_FIXTURES[@]}"
    "${PKGS_QA[@]}"
    "${PKGS_LINT[@]}"
)
if [ "$WITH_GUI" -eq 1 ]; then
    PKGS+=("${PKGS_GUI[@]}")
fi
if [ "$WITH_M3" -eq 1 ]; then
    PKGS+=("${PKGS_M3[@]}")
fi
if [ "$WITH_OPTIONAL" -eq 1 ]; then
    PKGS+=("${PKGS_OPTIONAL[@]}")
fi

if [ "$PRINT_ONLY" -eq 1 ]; then
    printf '%s\n' "${PKGS[@]}"
    exit 0
fi

# --- sanity checks ------------------------------------------------------------

if [ -f /run/.containerenv ] || [ -f /run/.toolboxenv ]; then
    echo "Run this script on the host, not inside a container." >&2
    exit 1
fi

if ! command -v toolbox >/dev/null 2>&1; then
    echo "toolbox is not installed on the host." >&2
    exit 1
fi

# --- container ----------------------------------------------------------------

if podman container exists "$CONTAINER" 2>/dev/null; then
    echo "Container '$CONTAINER' already exists; skipping creation."
else
    echo "Creating container '$CONTAINER' (Fedora $RELEASE)..."
    toolbox --assumeyes create --distro fedora --release "$RELEASE" "$CONTAINER"
fi

# --- packages -----------------------------------------------------------------

echo "Installing ${#PKGS[@]} packages inside '$CONTAINER'..."
toolbox run --container "$CONTAINER" \
    sudo dnf install --refresh --assumeyes --setopt=install_weak_deps=False "${PKGS[@]}"

# Keep the container current: backends parse untrusted files, so security
# fixes in the format libraries matter. Converges to "nothing to do".
# The environment is rolling, not pinned: versions drift with Fedora updates
# and may differ from CI between upgrades (ADR 0014, decision 11).
echo "Upgrading packages inside '$CONTAINER'..."
toolbox run --container "$CONTAINER" \
    sudo dnf upgrade --refresh --assumeyes

# --- report -------------------------------------------------------------------

echo
echo "Provisioned '$CONTAINER'. Versions (compare with CI's rpm-versions.txt):"
toolbox run --container "$CONTAINER" \
    rpm -q --qf '  %{NAME}-%{VERSION}-%{RELEASE}\n' \
    gcc-c++ clang clang-tools-extra clazy cppcheck reuse \
    kf6-kio-devel qt6-qtbase-devel libarchive-devel dolphin
