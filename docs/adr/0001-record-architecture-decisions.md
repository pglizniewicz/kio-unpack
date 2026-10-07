<!--
SPDX-License-Identifier: GPL-2.0-or-later
SPDX-FileCopyrightText: 2026 kio-unpack contributors
-->

# 0001. Record architecture decisions

Date: 2026-10-07
Status: Accepted

## Context

kio-unpack starts from an empty repository. The planning session settled many
questions by reading upstream source code (KIO, Dolphin, KCoreAddons,
kio-extras, Krusader, Ark, bit7z, 7-Zip, libmirage, libudfread, libarchive,
Flathub manifests) and by checking the development machine. These decisions rest
on facts that will drift: KIO internals, Flathub manifests, Fedora package
versions. Without a record, later contributors and coding agents would have to
rediscover why a choice was made, or would undo it without knowing what it
protected.

## Decision

Each significant decision gets an Architecture Decision Record in
`docs/adr/NNNN-short-title.md`, numbered sequentially. A record is written in
prose with the sections Context, Decision, Consequences and Alternatives
considered, and states its date and status. It cites primary sources as
`repo/path:line` together with the upstream commit the reading was based on, or
names the command that established a fact on the machine. Anything not checked
against a primary source is marked UNVERIFIED.

Until the repository is published (the first push to the public GitHub
repository, ADR 0013), the records describe the agreed state and are edited in
place. After publication an accepted record is not rewritten; a new record
supersedes it, and the old one's status line points to its successor.

Observable behaviour that follows from a decision goes into
`docs/requirements.md` as an EARS requirement with an ID; the ADR explains why,
the requirement says what, and the test proves it.

## Consequences

Decisions remain traceable from requirement to rationale to source. Records grow
stale as upstream code changes; the cited commit tells a reader how old the
evidence is.

## Alternatives considered

Keeping decisions only in `docs/architecture.md` was rejected because that
document describes the current design and would lose the reasons and the
rejected options as it is edited.
