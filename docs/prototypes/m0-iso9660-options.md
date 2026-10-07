<!--
SPDX-License-Identifier: GPL-2.0-or-later
SPDX-FileCopyrightText: 2026 kio-unpack contributors
-->

# ISO 9660 name flavours through libarchive options

Date: 2026-10-07. Environment: toolbx container `kio-unpack` (Fedora 44),
libarchive 3.8.7, xorriso from Fedora 44.

## Question

Do the libarchive format options `iso9660:!rockridge` and
`iso9660:!joliet` give the Joliet and the plain ISO 9660 names of an image
that carries Rock Ridge and Joliet? `docs/architecture.md`, section 4.4, and
the `[Volumes]` name flavours (REQ-062, M2) rely on it.

## Fixture

A directory `src/` with `Dir_With_Long_Name/a_long_file_name.txt` and a
symlink `link` to that file, written with
`xorriso -as mkisofs -quiet -R -J -o rrj.iso src`.

## Probe

Compiled with `cc -o lsiso lsiso.c -larchive` and run as
`./lsiso rrj.iso [options]`:

```c
#include <archive.h>
#include <archive_entry.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    struct archive *a = archive_read_new();
    archive_read_support_format_iso9660(a);
    if (argc > 2 && archive_read_set_options(a, argv[2]) != ARCHIVE_OK)
        fprintf(stderr, "options: %s\n", archive_error_string(a));
    if (archive_read_open_filename(a, argv[1], 10240) != ARCHIVE_OK) {
        fprintf(stderr, "open: %s\n", archive_error_string(a));
        return 1;
    }
    struct archive_entry *e;
    while (archive_read_next_header(a, &e) == ARCHIVE_OK) {
        const char *l = archive_entry_symlink(e);
        printf("%06o %s%s%s\n", (unsigned)archive_entry_mode(e), archive_entry_pathname(e), l ? " -> " : "", l ? l : "");
    }
    printf("format=%s\n", archive_format_name(a));
    archive_read_free(a);
    return 0;
}
```

## Results

| Options | Output | Result |
|---|---|---|
| none | `.`, `Dir_With_Long_Name`, `Dir_With_Long_Name/a_long_file_name.txt` (0644), `link -> Dir_With_Long_Name/a_long_file_name.txt`; format `ISO9660 with Rockridge extensions` | pass: Rock Ridge names, modes, symlink |
| `iso9660:!rockridge` | `.`, `Dir_With_Long_Name`, `Dir_With_Long_Name/a_long_file_name.txt` (0400); no `link`; format `ISO9660` | pass: Joliet names; the Joliet tree has no symlink |
| `iso9660:!rockridge,iso9660:!joliet` | `.`, `DIR_WITH`, `DIR_WITH/A_LONG_F.TXT` (0400), `LINK` as a regular file; format `ISO9660` | pass: plain ISO 9660 8.3 names |

Directories without Rock Ridge are reported as 040700, files as 100400.

## Decision

The options work as `docs/architecture.md`, section 4.4, describes. The
backend cannot tell Joliet from plain ISO 9660 through `archive_format()`,
which is `ISO9660` in both cases; it knows the flavour from the options it
set.
