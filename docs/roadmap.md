<!--
SPDX-License-Identifier: GPL-2.0-or-later
SPDX-FileCopyrightText: 2026 kio-unpack contributors
-->

# kio-unpack roadmap

Release 1.0 consists of the features of M0 to M3 (owner's decision,
2026-10-07). M4 is about distribution.

Milestones build on each other. Each lists its scope, the requirements it
delivers (`docs/requirements.md`), and acceptance criteria that are checked by
running the listed commands. Commands run from the repository root, in an
environment that has the dependencies from `docs/building.md`.

## Spikes before dependent work

A decision that depends on how KIO, Dolphin or Flatpak behave at run time is
settled by a spike before the work that depends on it: a small throwaway
program in `prototypes/` (built with `-DKIO_UNPACK_PROTOTYPES=ON`) or a
scripted check, run against the versions in the container and, where stated,
in the Flatpak. Until a spike is recorded, the requirements and scope items
that depend on it are provisional. A spike is done when all of the following
hold:

- its record `docs/prototypes/<spike>.md` lists the KIO, Dolphin, Qt and, where
  relevant, Flatpak runtime versions, every check with pass or fail and notes,
  and the decision;
- the dependent requirements are confirmed or replaced in
  `docs/requirements.md`, the affected ADR and `docs/architecture.md` are
  updated, and the open question is closed;
- the prototype code is deleted (the record stays). S-4 is the one
  exception: its decision is taken at the start of M0b, and its prototype
  stays until check C8 runs in M1.

Prototype code is throwaway: it builds with warnings as errors, but
`scripts/lint.sh` and the clazy build leave `prototypes/` out (ADR 0014).

The owner of every spike is the maintainer, who runs it or reviews the agent's
run and signs off the decision in the record.

| Spike | Question | Runs | Gates | Fallback if it fails |
|---|---|---|---|---|
| S-1 | OQ-2: how KIO's scheduler assigns host-less `unpack:` jobs to worker processes | start of M0b | `maxInstances` in `unpack.json`; `SessionCache` size and its usefulness (REQ-016, REQ-017, REQ-081) | `maxInstances` 1, `SessionCache` with the starting size from `docs/architecture.md`, section 9.1 |
| S-2 | whether a suspended KIO job blocks the worker's writes in `data()` (ADR 0011) | start of M0c | the watchdog's pause scope around client calls; the suspended-job case of REQ-096 | keep the pause scope (harmless either way) and drop that test case |
| S-3 | OQ-6: whether a worker's `messageBox()` reaches the user from a list, stat and get job started by Dolphin | start of M0b, with S-1 and S-4 | REQ-058 to REQ-060 (confirmation before costly decoding), the message-box case of REQ-096 | REQ-058 only for job types where the box appears; elsewhere REQ-060's warning |
| S-4 | OQ-11: whether `[Volumes]` as a directory with a link target behaves as ADR 0008 assumes | start of M0b, with S-1 | REQ-092, REQ-093 | ADR 0008's fallback |
| S-5 | OQ-10: whether an app-level `--env=QT_PLUGIN_PATH` overrides the runtime value, so an extension point can add KIO workers | M1, after the first successful build | ADR 0010's recommendation (c) for M4 | recommend route (a), upstreaming |
| S-6 | OQ-13: how many `get` and `listDir` requests Dolphin's previews send to a `:local` worker, and how files inside an image are opened in applications | start of M0b, with S-1 | the `[Nested] PresentAs` default (REQ-042); ADR 0005's rule that listings never decode entries | keep `Class` `:local`, describe Dolphin's preview settings in `README.md`; if folder previews list inside directories, `PresentAs` defaults to `File` |

**S-1 checks.** A worker build that logs its PID and the URL of every request;
in Dolphin: two tabs browsing two images, a large copy from one image while
the other tab lists, then the same with `maxInstances` 3. Record which process
serves which job, whether a running copy blocks listing in another tab, and how
often consecutive jobs on one image reach the same process.

**S-2 checks.** A prototype worker whose `get` sends 1 MiB chunks with a log
line before and after each `data()` call; suspend the job through `KJob::suspend()`
in a test program and through the pause button in Dolphin's progress view;
record whether `data()` blocks while the job is suspended.

**S-3 checks.** Recorded in `docs/prototypes/m0b-message-box.md`. The S-4
prototype worker calls `messageBox()` in `listDir`,
`stat` and `get` when an environment variable asks for it; in Dolphin (browse,
copy, properties dialog), `kioclient` and a test program with and without a
UI delegate, record where the box appears and what `messageBox()` returns
where it does not.

**S-4 checks.** In a running Dolphin and through the KIO API, check that a
directory with a link target behaves as ADR 0008 assumes. REQ-092 and REQ-093
stay provisional until the record exists.

- **Prototype.** A throwaway worker `kio_vproto` (scheme `vproto:`) in
  `prototypes/volumes-link/`, built only with `-DKIO_UNPACK_PROTOTYPES=ON`. It
  serves a fixed tree from memory: `a.txt`, `dir/b.txt` and `[Volumes]` at the
  root; `[Volumes]` is `S_IFDIR` with `UDS_LINK_DEST`, a display type and a
  `UDS_SIZE` equal to its text; it contains `One/x.txt` and `Two/y.txt`; `get`
  on `[Volumes]` returns the text. The environment variable `VPROTO_LINK_DEST`
  selects the `UDS_LINK_DEST` value: the entry's own absolute `vproto:` URL,
  `.`, or the plain word `Volumes`.
- **Checks**, for each `UDS_LINK_DEST` value, in Dolphin from the build tree:
  - C1: double-click enters `[Volumes]`, the location bar shows
    `vproto:/[Volumes]`, Back returns to the root, the link emblem is shown;
  - C2: select all at the root, Copy, Paste into an empty local directory:
    `a.txt` and `dir/b.txt` once, one regular file `[Volumes]` holding the
    text, no error dialog and no question;
  - C3: dragging `[Volumes]` alone into a local directory gives one regular
    file;
  - C4: `kioclient copy` of the root into a local directory gives the same
    result as C2;
  - C5: a small test program shows that `KIO::listRecursive` of the root
    returns no entry below `[Volumes]` and that `KIO::copy` with `[Volumes]` as
    the source gives one regular file;
  - C6: the properties dialog of `[Volumes]` opens without an error (note what
    it shows as the link target);
  - C7: opening `[Volumes]` in a new tab and by typing its URL works, and the
    Details view shows no error in the size column;
- **Record.** `docs/prototypes/m0b-volumes-link.md`: KIO, Dolphin and Qt
  versions, pass or fail per check and value, notes, the chosen value, and the
  decision.
- **Decision.** If C1 to C5 pass for at least one value, REQ-092 and REQ-093
  are confirmed with that value, written into REQ-092 and the UDS table in
  `docs/architecture.md`, and OQ-11 is closed. If C1 to C5 fail for every value,
  ADR 0008 switches to its fallback, REQ-092 and REQ-093 are replaced by the
  fallback's requirements, and the HFS decision of M2 becomes mandatory.
  Failures in C6 and C7 are recorded and discussed before deciding.
- **C8, in M1.** C1 and C2 are repeated in the Flatpak Dolphin of M1, whose
  Dolphin version differs from Fedora's, and added to the record; a failure
  reopens the decision before M2. The prototype is built into a separate
  branch of the app, never into `kio-unpack-dev` (M1, step 4).
- **Afterwards** the prototype directory is deleted once C8 is recorded; the
  REQ-092 and REQ-093 tests take its place in M2, the record stays.

**S-5 checks.** In a copy of the dev manifest, add `--env=QT_PLUGIN_PATH=` with
an extra directory in front of the runtime's value and install a test plugin
there; record the value inside the sandbox and whether `kioclient` finds the
plugin.

**S-6 checks.** `Class` `:local` makes KIO's preview job treat `unpack:` items
as local and fast: the `MaximumSize` limit (unlimited by default) applies
instead of `MaximumRemoteSize` (0 by default, no previews), and directories
are not skipped (`kio/src/gui/filepreviewjob.cpp:262-271`, KIO `69f11e2c`).
With the S-1 worker build, which logs every request, and the dev-loop Dolphin
with previews on: open the root of an ISO holding 20 images, a PDF, a large
file of a type with a thumbnailer and a subdirectory with images; record the
number of `get` and `listDir` requests and the bytes read, whether the
subdirectory gets a folder preview that lists it, and the same with previews
off. Open a file from inside the image with an application that accepts KIO
URLs and with one that does not; record whether it receives the `unpack:`
URL, a temporary copy or a kio-fuse path. Record the preview settings used, in
`docs/prototypes/m0b-previews.md`.

## M0: worker for ISO images and libarchive formats, in three stages

M0 is split into three vertical stages, each ending in a worker that builds,
passes its tests and can be tried in Dolphin, so integration problems show up
in the first stage instead of at the end. "M0" elsewhere in the documentation
means all three stages. Requirements carry their stage (`M0a`, `M0b`, `M0c`) in
`docs/requirements.md`. Closing a stage, or a later milestone, updates the box
"Current stage" at the top of `docs/architecture.md` to the next one.

### M0a: registration and a working path through ISO images

**Scope.** Public GitHub repository (ADR 0013), whose first commit already
carries `LICENSES/GPL-2.0-or-later.txt`, SPDX headers on every file we wrote
(ADR 0012) and a short `README.md` (purpose, `unpack:` URLs, the Dolphin
setting as REQ-005 requires, link to `docs/`); dependency list
(`docs/building.md`) with the Fedora/toolbx provisioning script; CMake skeleton
with ECM (`KDEInstallDirs6`, `KDECMakeSettings`, `KDECompilerSettings` level
6.13, deprecation limits, `KIO_UNPACK_WERROR`, `kde_clang_format`; ADR 0014),
because formatting and warnings are cheapest from the first line; the
`KIO_UNPACK_PROTOTYPES` option with an empty `prototypes/`, so the spikes at
the start of M0b can add their workers;
`TRANSLATION_DOMAIN=kio6_unpack` with every user-visible string in `i18n()`;
`libkio-unpack-core` with `FileByteSource`, `Sniff` (ISO 9660), `EntryTable`
and `PathResolver` without nesting and without a cache; the libarchive backend
for ISO 9660 with libarchive's own choice of names: Rock Ridge when present,
otherwise Joliet, otherwise plain ISO 9660 (`docs/architecture.md`, section
4.4); the other name flavours of an image become reachable under `[Volumes]`
in M2 (REQ-062); the worker (`kdemain`, `listDir`, `stat`, `get`,
`unpack.json`); `kio-unpack-cli`; tests with
fixtures generated by xorriso; a GitHub Actions workflow that builds and runs
`ctest` in a `fedora:44` container.

**Requirements.** REQ-001 to REQ-006, REQ-008, REQ-009, REQ-011, REQ-021 to
REQ-026, REQ-029, REQ-030, REQ-035, REQ-087, REQ-103.

**Steps.**

1. Install the dependencies listed in `docs/building.md`. On Fedora,
   `bash scripts/toolbox-setup.sh` provisions a toolbx container `kio-unpack`
   with all of them; then run the following steps inside it.
2. Build and test with the block "[M0a] Configure, build, test" of the
   commands in `AGENTS.md`, the single place for project commands.
3. Smoke test without a GUI with the block "[M0a] Smoke test the worker from
   the build tree, without a GUI" in `AGENTS.md`.
4. Start Dolphin from the build tree (ADR 0009) with the block "[M0a] Dev
   loop" in `AGENTS.md`. It uses its own configuration directory, so the
   user's `dolphinrc` stays untouched (a toolbx container shares it with the
   host), and its first command enables "Browse compressed files as folders"
   (off by default) in that directory, so a double-click on `rr.iso` opens it
   as a folder; typing the `unpack:` URL in the location bar works with or
   without it.
   This Dolphin and the workers it starts read every configuration file
   (`dolphinrc`, `kdeglobals`, `mimeapps.list`, `kio_unpackrc`) from
   `build/dev-config`, so it starts with default theme and file associations.

**Acceptance.**

The gate is automated; the Dolphin checks are a manual checklist on top of it
and never replace a failing test.

- Gate: `ctest` passes locally and in the GitHub Actions workflow, with
  warnings as errors. It includes the smoke tests of step 3 as `ctest` tests
  (`kioclient --platform offscreen` `stat`, `ls` and `cat` on generated
  fixtures, with `QT_PLUGIN_PATH` set to the build tree), so discovery of the
  worker through KIO is checked without a GUI, and a Joliet-only image
  (`xorriso -as mkisofs -J --norock`) listed with its Joliet names (REQ-025).
  `kioclient` needs no session bus for this: on 2026-10-07, in the toolbx
  container, `kioclient --platform offscreen ls trash:/` (an out-of-process
  worker) succeeded with `DBUS_SESSION_BUS_ADDRESS`, `DISPLAY`,
  `WAYLAND_DISPLAY` and `XDG_RUNTIME_DIR` unset. The same holds in the CI
  container: a replay of the workflow in a fresh `fedora:44` container, as
  root and without a session bus, passed every test including the smoke test
  (`docs/prototypes/m0a-review.md`, section "CI"), so `dbus-run-session` is
  not needed.
- Checklist, in the Dolphin of step 4 with its own `build/dev-config`
  profile: clicking `rr.iso` opens it as a folder; Rock Ridge names,
  permissions and symlinks are shown; copying a file out yields a
  byte-identical file. The result, with the Dolphin and KIO versions, goes
  into the pull request or commit that closes M0a.
- Specification review, after the gate passes locally and before the first
  push: every statement in `docs/architecture.md`, `docs/requirements.md` and
  the ADRs that M0a's code touched (plugin build and discovery, worker
  start-up, libarchive's ISO 9660 reading, the dev loop) is confirmed or
  corrected in place, which ADR 0001 allows only until the first push. The
  result goes into `docs/prototypes/m0a-review.md`: each statement checked,
  confirmed or changed, and where.
- `README.md` and `LICENSES/GPL-2.0-or-later.txt` are in the first pushed
  commit.
- No host package changed (`rpm -qa` before and after is identical apart from
  ordinary updates).

### M0b: errors, paths, names and the session cache

**Scope.** Spikes S-1 (OQ-2), S-3 (OQ-6), S-4 (OQ-11) and S-6 (OQ-13)
first ("Spikes before dependent work"): S-1 sets `maxInstances` and the
`SessionCache` size, S-3 and S-4 settle the message boxes and `[Volumes]` of
M2 early, S-6 records the load Dolphin's previews put on the worker; path resolution with all its errors and the redirect to `file:`;
`guard()` and the full error table; `CachedByteSource` and `SessionCache` with
re-validation and the idle drop; symlink handling for `get`; cancellation in
every loop; the other archive formats libarchive reads (ZIP, tar and the rest)
with name decoding (single header pass, legacy charsets from the system
language, `\` in ZIP names); the configuration loader with `[Names]` and
`[Limits] IdleSeconds`;
`scripts/extract-messages.sh`, `po/` and the Polish catalogue (ADR 0013).

**Requirements.** REQ-010, REQ-012 to REQ-019, REQ-027, REQ-028, REQ-031 to
REQ-034, REQ-036 to REQ-039, REQ-077, REQ-081, REQ-090, REQ-098, REQ-099,
REQ-101, REQ-102.

**Acceptance.**

- `ctest` passes locally and in the workflow.
- An `unpack:` URL of a plain directory redirects to `file:`.
- A garbage file named `bad.iso` shows Dolphin's error bar, which offers to open
  it with the default application for `application/vnd.efi.iso` (Ark or ISO Image
  Writer, depending on associations).
- Paths that are missing, unreadable, FIFOs or contain control characters give
  the KIO errors of the error table, never a crash or an empty view.
- A ZIP with CP852 names shows correct Polish names on a Polish system.
- A tar and a ZIP open as folders when their `unpack:` URL is typed into
  Dolphin's location bar. A double-click on a local `.zip` or `.tar` still
  opens it through kio-extras (with the browse setting on) or Ark, because
  the worker does not claim their MIME types (REQ-004); the context-menu
  action for them comes in M2 (REQ-007).
- Cancelling a large copy ends the transfer after at most one further data
  chunk (REQ-077); the worker never needs KIO's own kill five seconds after
  `SIGTERM`.

### M0c: hardening and quality checks

**Scope.** Spike S-2 first, which decides how the watchdog treats suspended
jobs; `kdemain` resource limits and signal handling, the watchdog
(ADR 0011); entry and string-arena caps; the `[Limits]` keys of REQ-083;
`BufferByteSource` and the libFuzzer harnesses; the sanitizer build in CI;
the rest of ADR 0014: `.clang-tidy`, cppcheck, the clazy build, REUSE
(`reuse lint`, and `REUSE.toml` for files that cannot carry headers and for
the Flathub files in `flatpak/`, which carry no license; `LICENSES/` exists
since M0a), codespell and markdownlint configuration, the
coverage build, `scripts/lint.sh`, `scripts/pre-commit.sh`, and the workflow
extended with lint, sanitizer, clazy, fuzz (build and a 60-second run per
harness) and coverage jobs; a complete Polish catalogue. From M0c on, every
parser of our own lands in the same change as its fuzz harness (ADR 0011).

**Requirements.** REQ-078 to REQ-080, REQ-082, REQ-083, REQ-091, REQ-095,
REQ-096.

**Acceptance.**

- `ctest` passes, also in the sanitizer build.
- A worker blocked by a fault-injecting backend ends without a crash dialog,
  and an idle worker is never ended by the watchdog.
- Every fuzz harness builds and runs for one minute without a finding.
- `bash scripts/lint.sh` and the clazy build pass with no findings, and every
  job of the GitHub Actions workflow passes (ADR 0014).
- `msgfmt --check` passes and every string has a Polish translation.

**Known limitation (all of M0).** Every `get` reopens the image and walks the
headers up to its entry (`docs/architecture.md`, section 4.4, libarchive), so
copying M files out of an image with N entries costs on the order of M × N
header reads. Copying many files from a large ISO is slow in M0; that is
expected and not a regression. Direct access through data locators comes in M2
(ADR 0006).

## M1: the same worker as a Flatpak

**Scope.** Build Dolphin from the derived manifest in `flatpak/` with our module
appended, on the host through `org.flatpak.Builder` (ADR 0010); spike S-5
(OQ-10) once the build works.

**Requirements.** REQ-088, REQ-089, and REQ-013 under Flatpak.

**Steps.**

0. One-time host setup (user-level Flatpaks only) and SDK check:
   ```sh
   flatpak install --user flathub org.flatpak.Builder org.kde.Sdk//6.10 org.kde.Platform//6.10
   flatpak run --command=ls org.kde.Sdk//6.10 /usr/include/archive.h /usr/lib/x86_64-linux-gnu/pkgconfig/libarchive.pc
   ```
   The upstream Ark module requires libarchive and the manifest builds none, so
   the check is expected to pass; if it fails, add a libarchive module before
   continuing.
1. Check that every source is pinned, download all sources in a separate step,
   then build and install the dev branch with downloads disabled
   (`flatpak/README.md`):
   ```sh
   flatpak run --command=flatpak-builder-lint org.flatpak.Builder manifest flatpak/org.kde.dolphin.json
   flatpak run org.flatpak.Builder --download-only --state-dir=.flatpak-builder flatpak/build flatpak/org.kde.dolphin.json
   flatpak run org.flatpak.Builder --user --install --force-clean --ccache --disable-download --state-dir=.flatpak-builder --repo=flatpak/repo flatpak/build flatpak/org.kde.dolphin.json
   ```
2. Verify inside the sandbox:
   ```sh
   flatpak run --branch=kio-unpack-dev --command=sh org.kde.dolphin -c 'ls /app/lib/plugins/kf6/kio; echo $QT_PLUGIN_PATH'
   flatpak run --branch=kio-unpack-dev --command=kioclient org.kde.dolphin --platform offscreen ls unpack:$HOME/kio-unpack-samples/rr.iso
   flatpak run --branch=kio-unpack-dev org.kde.dolphin
   ```
3. Spike S-5 (OQ-10, "Spikes before dependent work"), recorded in
   `docs/prototypes/m1-flatpak-plugin-path.md`.
4. Check C8 of spike S-4: in a copy of the dev manifest whose `"branch"` is
   `kio-unpack-proto` and whose `kio-unpack` module is configured with
   `-DKIO_UNPACK_PROTOTYPES=ON`, built into a build directory and `--repo`
   of its own, repeat C1 and C2 with the `kio_vproto` prototype and add
   the result to `docs/prototypes/m0b-volumes-link.md`. The prototype worker
   then never lands in the `kio-unpack-dev` branch. Uninstall the
   `kio-unpack-proto` branch afterwards (`flatpak/README.md`, "Remove the dev
   build").

**Acceptance.**

- `kio_unpack.so` is present in `/app/lib/plugins/kf6/kio` and KIO uses it.
- Listing and copying out of an ISO in `$HOME` work in the sandboxed Dolphin.
- An image in a path the sandbox does not grant (for example `/opt` or `/srv`)
  produces a KIO error, not a crash or an empty view: `ERR_DOES_NOT_EXIST` when
  the path does not exist inside the sandbox, `ERR_ACCESS_DENIED` for
  `EACCES` (REQ-013); `docs/prototypes/m1-flatpak-plugin-path.md` notes
  which one appeared.
- The build needs no network access: after the `--download-only` step it
  succeeds with `--disable-download`, and the lint reports no
  `module-*-source-git-*` error.
- `flatpak run --command=flatpak-builder-lint org.flatpak.Builder manifest flatpak/org.kde.dolphin.json`
  reports nothing that blocks a local build.
- After a change to kio-unpack only, a rebuild recompiles only the `kio-unpack`
  module.

## M2: coverage

**Scope.** UDF backend with libudfread built from a pinned commit plus our
metadata patch, natively through `scripts/build-deps.sh` and in the Flatpak as
`flatpak/modules/libudfread.json` (ADR 0003); our own MBR, GPT and
APM partition parser and El Torito parser; sniffing that recognises isohybrid
images and reports HFS, HFS+, DMG, SquashFS, WIM and virtual-disk formats as not
supported; priority table and fallback chain; `[Volumes]` in the form decided
by spike S-4 (ADR 0008); nested archives with
the data strategy of ADR 0006, including `DecodingByteSource`,
ISO 9660, tar and ZIP data locators and a checkpointing decoder on zlib and
liblzma (ADR 0006);
`UnpackStats`; `SparseByteSource` for tests and the periodic stress workflow
(ADR 0014); `[Nested] PresentAs` setting and the `kio-unpack-action` helper
with its Dolphin service menus (ADR 0005, ADR 0007); solid-block handling (ADR 0006); MIME
list expansion (OQ-1); corpus scripts.

**Requirements.** REQ-007, REQ-020, REQ-040 to REQ-067, REQ-084 to REQ-086,
REQ-094, REQ-100; REQ-092 and REQ-093 or their replacements, as decided by
spike S-4.

**Acceptance.** Requirements' tests pass natively and the Flatpak build still
succeeds with the libudfread module; the corpus smoke run over the downloaded
sample shortlist reports no crash and no timeout.

**Decide during M2.** Whether to list HFS and HFS+ volumes under `[Volumes]` as
regular files whose `get` returns the raw partition bytes, instead of
directories that fail with `ERR_WORKER_DEFINED` when entered (REQ-065).
Copying everything selected inside `[Volumes]` on a hybrid Mac/PC disc then
saves a mountable HFS image instead of stopping on an error. Copying the image
root is not affected while `[Volumes]` is a link-directory; if the OQ-11
prototype fails and ADR 0008's fallback makes `[Volumes]` a plain directory,
this change becomes necessary, because copying a whole hybrid disc would then
reach the HFS volume.

## M3: unusual images and hardening

**Scope.** Own optical-image layer (ADR 0004): sector-geometry model for all CD
sector sizes, parsers for CUE/BIN, raw dumps, MDS/MDF, CCD/IMG/SUB and TOC/BIN;
tracks and sessions as volumes; each descriptor parser with its fuzz harness.
Before M3 the worker reads only images with 2048-byte sectors and neither assembles nor special-cases
Mode 2 Form 2 data; other sector sizes (REQ-069) and the Form 2 rule
(REQ-070) start here.

BIN/CUE support is a must-have for 1.0 (REQ-068, REQ-072); local `.cue` files
are browsed through a context-menu action, not by default (ADR 0007). Audio tracks of optical images appear as WAV files (REQ-071).

**Requirements.** REQ-068 to REQ-076, REQ-097.

**CI.** From M3 on the workflow installs the m3 package profile as well
(`bash scripts/toolbox-setup.sh --ci --m3 --print-packages`), because the M3 tests
generate fixtures with vcdimager and FFmpeg (ADR 0014).

## M4: distribution

**Scope.** If spike S-5 (OQ-10) confirmed it, propose an extension point for
KIO workers in the Flathub Dolphin manifest, demonstrated with the M1 harness
(ADR 0010, option c), otherwise propose upstreaming (option a); propose a
deterministic tie-break for `protocolForArchiveMimetype()` upstream in KIO
(ADR 0007); fall back to upstreaming a libarchive-only subset or a module PR.

## Later ideas (not planned)

- A context-menu action that converts WAV audio tracks to FLAC or MP3 while
  copying them out.
- libmirage for the long tail of optical formats (NRG, CDI, MDX, B6T, C2D, CIF,
  DAA, ISZ, ECM, CSO, CHD with libchdr), run in a seccomp-confined helper process
  (ADR 0004 explains why it is not in 1.0).
- Splitting ripped albums stored as one compressed file (FLAC/MP3 plus CUE) into
  tracks, which needs a decoder such as libsndfile. Not in 1.0 (owner's decision,
  2026-10-07); albums with one file per track are listed as they are (REQ-076).
- HFS and HFS+ for the Mac side of hybrid discs; deferred beyond 1.0. Options
  studied on 2026-10-07 for classic HFS: our own bounds-checked read-only reader
  (estimated 800-1200 lines, with CiderPress2, machfs and Aaru's LGPL-2.1+ code as
  references) or vendored libhfs (hfsutils 3.2.6, 1998, unfuzzed, path-only I/O);
  Aaru 5.4.2 only identifies HFS, its 6.0 beta reads it but is .NET with prebuilt
  native packages; libfshfs and XADMaster do not read classic HFS.
- unar/lsar (StuffIt, LHA, RAR through The Unarchiver's CLI) and Aaru as optional
  external backends; deferred beyond 1.0 (owner's decision, 2026-10-07).
