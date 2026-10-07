<!--
SPDX-License-Identifier: GPL-2.0-or-later
SPDX-FileCopyrightText: 2026 kio-unpack contributors
-->

# 0011. Robustness and untrusted names

Date: 2026-10-07
Status: Accepted

## Context

Every file the worker opens is untrusted, and parsers for old disc and archive
formats are a known source of memory-safety bugs (for example, Fedora's 7-Zip
26.03 update fixed code execution in an NTFS handler, `rpm -q --changelog 7zip`).
Names inside archives are fully controlled by their authors.

What KIO provides: every worker except `file` and `admin` runs in its own process
(`kio/src/core/worker.cpp:469-477`); if it dies, the client gets
`ERR_WORKER_DIED` and continues (`worker.cpp:305-326`). On cancel, KIO sends
`SIGTERM`; the worker's handler sets the kill flag and arms `alarm(5)`
(`kio/src/core/slavebase.cpp:229-243`). Out-of-process workers initialise KCrash
(`slavebase.cpp:255-261`). Idle workers are killed after three minutes
(`scheduler.cpp:28-29`). KIO has no client-side response timeout
(`workerbase.h:763-801`), so a hung worker blocks its job until the user cancels.

libarchive decodes names by the format's own rules (ZIP UTF-8 flag and Info-ZIP
Unicode Path field, `archive_read_support_format_zip.c:1107-1150,1323-1336`), but
on Linux it does not convert ZIP names without the UTF-8 flag
(`archive_string.c:1854-1860`), so old CP437 or CP852 names come out as invalid
UTF-8 and `archive_entry_pathname_utf8()` returns nothing (`archive_entry.c:612-622`).
Its `hdrcharset` option sets the source charset for ZIP and tar
(`archive_read_support_format_zip.c:3763-3778`), but for every ZIP name without
the UTF-8 flag, including names that are valid UTF-8
(`archive_read_support_format_zip.c:1323-1336`: flag → UTF-8, else
`hdrcharset`, else the default conversion set up at `:1263-1268`). Info-ZIP on Unix and, from memory (UNVERIFIED), the macOS archiver
write UTF-8 names without the flag, so a code page forced through `hdrcharset`
garbles them. Without `hdrcharset` the default conversion on non-Windows is
`NULL` (`archive_string.c:1856-1860`), and with `NULL` the name is copied byte
for byte (`archive_string.c:4278-4284`), so `archive_entry_pathname()` returns
the raw bytes. libarchive citations are at commit `8bb3bbdc`.

Ark reads ZIP names through libzip with `ZIP_FL_ENC_GUESS`
(`ark/plugins/libzipplugin/libzipplugin.cpp:466-472,740`, Ark `142d6fb1`),
which keeps valid UTF-8 and otherwise converts from CP437 (libzip `0c69763e`,
`man/zip_get_name.mdoc:70-74`, `lib/zip_utf-8.c:103`, `lib/zip_string.c:97-99`),
or through libarchive's wide names
(`ark/plugins/libarchive/libarchiveplugin.cpp:598`); it has no setting. 7-Zip
(`9128b80e`) uses the UTF-8 flag, then the Unicode Path field, then a code page
given by the user; the Windows build then reads names from Unix hosts as UTF-8
(`CPP/7zip/Archive/Zip/ZipItem.cpp:405-450`), and otherwise it uses the system's
OEM code page for archives made on FAT, NTFS or Unix hosts and the ANSI code
page for others (`ZipItem.h:338-351`), so Polish DOS archives decode correctly
on a Polish Windows.

## Decision

1. **Process isolation** is KIO's; no helper process in 1.0.
2. **Exception firewall:** every `WorkerBase` virtual runs inside `guard()`, which
   maps our exceptions, `std::bad_alloc` and any `std::exception` to the KIO
   error table.
3. **Resource limits** set in `kdemain` before any backend is used:
   `RLIMIT_DATA` (default in `docs/architecture.md`, section 9.1),
   `RLIMIT_FSIZE` equal to the temporary-file cap, `RLIMIT_CPU` as a last
   resort; skipped in sanitizer builds; configurable.
   The limits only back up our own checks: temporary bytes are counted in user
   space (item 6) and writing stops before a cap is exceeded. The kernel signals
   that these limits raise would otherwise end the worker in the wrong way
   (POSIX `setrlimit`, Linux `getrlimit(2)`): exceeding `RLIMIT_FSIZE` sends
   `SIGXFSZ` and exceeding the soft `RLIMIT_CPU` sends `SIGXCPU`, and the default
   action of both is to terminate with a core dump. So `kdemain` sets `SIGXFSZ`
   to `SIG_IGN`, which makes the write fail with `EFBIG` (mapped to
   `ERR_DISK_FULL`), and installs a `SIGXCPU` handler that does what the
   watchdog does, with async-signal-safe calls only: `write()` one line to
   stderr, then `_exit()`. The hard CPU limit is a few seconds above the soft
   one, so a process that does not exit gets `SIGKILL`, which produces no core
   dump either.
4. **Watchdog thread:** an absolute deadline for open and list, a stall deadline
   renewed by every data callback for extraction. On expiry it writes one line to
   stderr and calls `_exit()`, so KIO reports `ERR_WORKER_DIED`. Never `abort()`
   (KCrash would show a crash dialog), never `alarm()` (KIO owns `SIGALRM`).
   The deadline exists only while an operation runs our code:
   - `guard()` arms it through an RAII scope on entry to every `WorkerBase`
     virtual and disarms it in the scope's destructor, before control returns
     to KIO's dispatch loop. A disarmed watchdog waits on a condition variable
     without a timeout, so an idle worker blocked on the IPC socket is never
     ended.
   - Arm, disarm and the expiry decision happen under one mutex. Every arm
     increments a generation number; the watchdog acts only if, under that
     mutex, the same generation is still armed and its deadline has passed.
     Stall renewals from data callbacks only store a timestamp in an atomic, so
     the data path takes no lock; the watchdog re-reads it before acting.
   - Calls that send to or wait for the client (`data()`, `listEntry()`,
     `listEntries()`, `messageBox()`, `warning()`) run inside a pause scope:
     the clock stops and the deadline starts again when the call returns. A
     message box (REQ-058) waits for the user, and a client may stop reading,
     for example while a copy job is paused, which blocks the worker's writes
     (UNVERIFIED for paused jobs; spike S-2 at the start of M0c checks it).
   - The watchdog is independent of `setTimeoutSpecialCommand(90, "idle")`. That
     timer is KIO's and fires from its dispatch loop; the `special("idle")` it
     triggers runs inside `guard()` like any other operation.
5. **Cooperative cancellation:** loops poll `wasKilled()`; data sinks return
   false to stop readers.
6. **Caps** on entries, name storage, nesting depth, temporary bytes per file
   and per session; declared sizes are hints, actual bytes are counted.
7. **Untrusted names:** empty, `.` and `..` components are dropped so no entry
   escapes its directory. Archives are opened without `hdrcharset`; the one
   header pass that builds the `EntryTable` keeps names that are valid UTF-8
   (flagged, from the Unicode Path field, or unflagged) and stores the raw
   bytes of the others. After the pass those names are decoded with `iconv`
   from `[Names] LegacyCodepage` (ZIP) or `[Names] LegacyUnixCharset` (tar,
   cpio, Rock Ridge). Unset keys follow the system language, as 7-Zip follows
   the system's OEM code page: CP852 and ISO-8859-2 for Polish, CP437 and
   ISO-8859-1 when the language is not in the table (REQ-099). Nothing is
   reopened, and unflagged UTF-8 names stay intact. A legacy name that happens
   to be valid UTF-8 is taken as UTF-8, as libzip does; this is rare for real
   words. libarchive converts flagged UTF-8 names to the locale's charset, so
   `kdemain` sets `LC_CTYPE` to `C.UTF-8` when the inherited locale is not
   UTF-8 (whether a Flatpak can inherit a non-UTF-8 locale is UNVERIFIED).
   Bytes the charset cannot decode and control characters become U+FFFD;
   names that collide get ` (2)`, ` (3)`, …; symlink targets are never followed
   out of their volume.
   Windows archivers often write ZIP names with `\` as the separator.
   libarchive turns them into `/` only when the "version made by" host is 0
   (MS-DOS), the name has no `/` and converts to a wide string
   (`archive_read_support_format_zip.c:1379-1395`, `zip_read_local_file_header`,
   libarchive `8bb3bbdc`). Its API does not report the host, so the ZIP
   backend applies the same rule to every ZIP entry after name decoding: a
   name with `\` and no `/` uses `\` as the separator. This runs before empty,
   `.` and `..` components are dropped, so `..\..\x` cannot escape. The rare
   ZIP made on Unix with a literal `\` in a top-level name is shown as folders;
   accepted (REQ-098).
8. **Fuzzing:** libFuzzer harnesses with ASan and UBSan for the sniffer, every
   own parser (partitions, El Torito, locators, optical descriptors), the
   checkpointing decoder, each backend through an in-memory source, and the
   resolver (`-DKIO_UNPACK_FUZZ=ON`, clang and compiler-rt from the distribution). A corpus
   smoke script tabulates crashes and timeouts on downloaded samples. Each
   parser of our own lands in the same change as its harness, and CI runs
   every harness for 60 seconds on each push (REQ-082), so no parser waits
   for a later milestone to be fuzzed.

## Consequences

A broken image, or a malicious one that triggers a crash, a hang or runaway
memory or CPU use, kills at most one worker process; the user sees an error
bar, not a frozen Dolphin or a crash dialog. These measures do not contain a
memory-safety bug that an image turns into code execution: the worker then
runs code with the user's rights. Under Flatpak it inherits Dolphin's
permissions, which include `--share=network` and
`--talk-name=org.freedesktop.Flatpak` (`flatpak/org.kde.dolphin.json:15,20`,
taken from the Flathub manifest); the second lets a process start commands
on the host through `flatpak-spawn --host` (UNVERIFIED, from memory), so the sandbox does not contain such code either. The defence
against that case is in our parsers: bounds checks, fuzzing and the
sanitizer build. Old ZIPs with Polish names
are readable on a Polish system without configuration; an archive from another
language area needs `LegacyCodepage` once, as in 7-Zip. Archives with UTF-8
names but no flag stay readable. Limits may reject
legitimate huge images; the message names the limit, and limits are
configurable. Fuzzing instruments our code but not libarchive or libudfread
unless they are rebuilt with sanitizers (OQ-4).

## Alternatives considered

- **`RLIMIT_AS`:** counts address-space reservations that are not memory use.
- **`alarm()` timeouts or `abort()` on hang:** conflict with KIO's `SIGALRM` and
  trigger KCrash.
- **A seccomp-confined helper process:** more isolation, more IPC, unverified in
  Flatpak; no longer needed once libmirage was dropped (ADR 0004).
- **Statistical charset guessing for names (`KEncodingProber`):** unreliable on
  short file names; UTF-8 validity plus a code page from the system language is
  predictable.
- **Reopening with `hdrcharset` when a name is not UTF-8:** a second header pass,
  and it garbles unflagged UTF-8 names in the same archive.
- **Asking the user in a dialog:** `WorkerBase` offers only message boxes with
  fixed buttons and a login/password dialog (`kio/src/core/workerbase.h:269-278,
  913`, KIO `69f11e2c`), no
  list to choose from, and whether Dolphin's list jobs can show worker dialogs
  at all is OQ-6.
