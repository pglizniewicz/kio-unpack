<!--
SPDX-License-Identifier: GPL-2.0-or-later
SPDX-FileCopyrightText: 2026 kio-unpack contributors
-->

# kio-unpack

kio-unpack is a read-only KIO worker for KDE Frameworks 6. It lets Dolphin and
every other KIO client browse disc images and archives as folders and copy
files out of them, without mounting anything and without root.

Status: early development. The current build reads ISO 9660 images with Rock
Ridge and Joliet names. UDF, partitions, El Torito boot images, nested
archives, the other archive formats and optical images (CUE/BIN, MDS/MDF, CCD,
TOC) follow; see [`docs/roadmap.md`](docs/roadmap.md).

## `unpack:` URLs

The path of an `unpack:` URL is the absolute local path of an image, followed
by a path inside it:

```text
unpack:/home/user/disc.iso
unpack:/home/user/disc.iso/dir/file.txt
```

Only images and archives on the local file system can be opened. An image on a
remote location such as `sftp:` or `smb:` has to be copied to a local folder
first.

## Dolphin's "Browse compressed files as folders"

Dolphin's setting "Browse compressed files as folders" (General > Behavior,
disabled by default) affects only one thing: whether a double-click on a local
image or archive opens it as a folder. Everything else works regardless of
that setting:

- the context-menu action "Open as Folder (kio-unpack)" (planned);
- `unpack:` URLs typed into the location bar;
- other KIO clients, such as `kioclient` and file dialogs;
- archives and images nested inside an `unpack:` folder (planned).

## Building

Dependencies are listed in [`docs/building.md`](docs/building.md). The worker is
developed and tested from the build tree, without installing it (ADR 0009); the
commands are in [`AGENTS.md`](AGENTS.md), section "Commands".

## Documentation

- [`docs/architecture.md`](docs/architecture.md): design
- [`docs/requirements.md`](docs/requirements.md): observable behaviour
- [`docs/adr/`](docs/adr/): architecture decisions
- [`docs/roadmap.md`](docs/roadmap.md): milestones

## License

GPL-2.0-or-later, see [`LICENSES/GPL-2.0-or-later.txt`](LICENSES/GPL-2.0-or-later.txt).
