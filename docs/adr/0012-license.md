<!--
SPDX-License-Identifier: GPL-2.0-or-later
SPDX-FileCopyrightText: 2026 kio-unpack contributors
-->

# 0012. Project license

Date: 2026-10-07
Status: Accepted

## Context

kio-unpack links or adapts these components:

| Component | License | Use |
|---|---|---|
| KDE Frameworks, Qt | LGPL family | linked |
| libarchive | BSD-2-Clause, some BSD-3 files (Fedora package metadata) | linked, from the distribution / KDE runtime |
| libudfread | LGPL-2.0-or-later (Fedora package metadata) | built by us from a pinned commit plus our patch (ADR 0003) |
| zlib (zlib-ng-compat), liblzma, libbz2, libzstd | Zlib; 0BSD; bzip2-1.0.6 (Fedora metadata says BSD-4-Clause, see below); BSD-3-Clause OR GPL-2.0-only (Fedora package metadata) | linked, from the distribution / KDE runtime |
| libmirage (cdemu) | GPL-2.0-or-later (`cdemu/libmirage/COPYING`, `mirage/context.c:7-8`) | source of adapted parser code (ADR 0004), not linked |

7-Zip (LGPL-2.1-or-later with the unRAR restriction on its RAR code,
`7zip/DOC/License.txt:10-30,145-149`), bit7z (MPL-2.0) and libmirage as a
library are not used (ADR 0002). libarchive's own RAR and RAR5 readers are
independent BSD-licensed code. KDE's licensing policy accepts GPL-2.0-or-later. Fedora labels libbz2
`BSD-4-Clause` (`rpm -q --qf '%{LICENSE}' bzip2-libs`), but the shipped text
(`/usr/share/licenses/bzip2-libs/LICENSE`, bzip2 1.0.8) is the SPDX license
`bzip2-1.0.6`: four conditions (retain the notice; do not misrepresent the
origin, acknowledgment appreciated but not required; mark altered versions; no
endorsement) and no advertising clause. These conditions are of the same kind as
the zlib license, which the FSF lists as GPL-compatible
(`https://www.gnu.org/licenses/license-list.html`); the FSF list does not name
bzip2 itself, so compatibility rests on this reading of the text.

## Decision

kio-unpack is licensed **GPL-2.0-or-later**. The license text is in
`LICENSES/GPL-2.0-or-later.txt` from the first commit, and every file we write
carries SPDX headers (`REUSE.toml` covers files that cannot, from M0c). Code adapted from libmirage keeps its original copyright notices and
license, which is the same. Patches to libudfread are offered upstream under
libudfread's license. New dependencies must be compatible with
GPL-2.0-or-later without forcing GPL-3; libraries licensed GPL-3.0-only,
GPL-3.0-or-later or LGPL-3.0 need an ADR before use.

## Consequences

The licensing is simple for distributions, Flathub and a possible move to KDE
(ADR 0010). Formats whose best libraries are GPL-3 or LGPL-3 (WIM through wimlib,
SquashFS through squashfs-tools-ng, libyal formats) stay out until a decision
accepts that.

## Alternatives considered

- **LGPL core with GPL parts:** allows reuse by non-GPL code at the cost of
  license bookkeeping; no reuse case exists.
- **GPL-3.0-or-later:** would admit wimlib and other GPL-3 libraries, but blocks
  GPL-2-only consumers; not needed for 1.0.
