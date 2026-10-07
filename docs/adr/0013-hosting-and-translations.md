<!--
SPDX-License-Identifier: GPL-2.0-or-later
SPDX-FileCopyrightText: 2026 kio-unpack contributors
-->

# 0013. Hosting on GitHub, translations with KI18n

Date: 2026-10-07
Status: Accepted

## Context

The project needs a public home and a translation mechanism. KDE projects on KDE
Invent get translations through KDE's infrastructure; projects hosted elsewhere
maintain their catalogues themselves. KIO workers use KI18n with a per-component
domain: kio-extras sets `TRANSLATION_DOMAIN="kio6_archive"`
(`kio-extras/archive/CMakeLists.txt:1`) and installs catalogues with
`ki18n_install(po)` (`kio-extras/CMakeLists.txt:252`). The macro is available on
the development machine (`/usr/lib64/cmake/KF6I18n/KF6I18nMacros.cmake:80-169`), as are
`xgettext` and `msgfmt`.

## Decision

- The repository is hosted on **GitHub**, created when planning ends. Moving to
  KDE Invent later stays possible; no GitHub-only feature may be required for
  building.
- All user-visible strings in the worker, the helper `kio-unpack-action` and the
  core go through **KI18n** with the domain **`kio6_unpack`**; the core uses
  `ki18n` through a thin wrapper so it stays free of widget dependencies.
- Catalogues live in the repository: `po/kio6_unpack.pot` from
  `scripts/extract-messages.sh`, one `po/<lang>/kio6_unpack.po` per language,
  installed by `ki18n_install(po)`. Polish is maintained from M0.
- Service-menu `.desktop` files carry translations inline (`Name[pl]=`).
- Messages are full sentences with `%1` placeholders and `i18nc` context for short
  labels, never concatenated.

## Consequences

Errors Dolphin shows appear in the user's language. The project regenerates its
template when strings change; a CI check can compare it with the sources. A move
to KDE Invent would replace `po/` with KDE's process.

## Alternatives considered

- **KDE Invent from the start:** KDE translations and review, but onboarding
  before there is code.
- **Untranslated strings:** error texts are user-facing.
