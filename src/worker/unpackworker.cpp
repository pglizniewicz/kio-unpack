/*
    SPDX-License-Identifier: GPL-2.0-or-later
    SPDX-FileCopyrightText: 2026 kio-unpack contributors
*/

#include "unpackworker.h"

#include "core/error.h"

#include <KLocalizedString>

#include <QDateTime>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QUrl>

#include <sys/stat.h>

#include <new>

using namespace KioUnpack;

namespace
{

// Size of the data() chunks of get(); it also bounds how much is sent after a
// cancellation (docs/architecture.md, section 9.1: starting value 1 MiB).
constexpr qsizetype kDataChunkSize = 1024 * 1024;
// stat() with StatMimeType sniffs the MIME type from this many leading bytes
// (docs/architecture.md, section 10).
constexpr qsizetype kMimeSniffSize = 64 * 1024;
// Directories are read-only: only read and execute bits are reported
// (docs/architecture.md, section 10).
constexpr std::uint32_t kDirectoryAccessMask = 0555;

mode_t fileTypeOf(EntryType type)
{
    switch (type) {
    case EntryType::Directory:
        return S_IFDIR;
    case EntryType::Symlink:
        return S_IFLNK;
    case EntryType::File:
    case EntryType::Other:
        return S_IFREG;
    }
    return S_IFREG;
}

QString nameOf(const EntryTable &table, EntryTable::Index index)
{
    return QString::fromUtf8(table.name(index));
}

} // namespace

UnpackWorker::UnpackWorker(const QByteArray &poolSocket, const QByteArray &appSocket)
    : KIO::WorkerBase(QByteArrayLiteral("unpack"), poolSocket, appSocket)
{
}

template<typename Operation>
KIO::WorkerResult UnpackWorker::guard(const QUrl &url, Operation &&operation)
{
    try {
        return operation();
    } catch (const Error &error) {
        return fail(url, error);
    } catch (const std::bad_alloc &) {
        return KIO::WorkerResult::fail(KIO::ERR_OUT_OF_MEMORY, url.toDisplayString());
    } catch (const std::exception &exception) {
        return KIO::WorkerResult::fail(KIO::ERR_WORKER_DEFINED,
                                       i18n("Internal error while reading %1: %2", url.toDisplayString(), QString::fromLocal8Bit(exception.what())));
    } catch (...) {
        return KIO::WorkerResult::fail(KIO::ERR_WORKER_DEFINED, i18n("Internal error while reading %1.", url.toDisplayString()));
    }
}

KIO::WorkerResult UnpackWorker::fail(const QUrl &url, const Error &error)
{
    const QString subject = url.toDisplayString();
    switch (error.kind()) {
    case ErrorKind::NotFound:
        return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, subject);
    case ErrorKind::AccessDenied:
        return KIO::WorkerResult::fail(KIO::ERR_ACCESS_DENIED, subject);
    case ErrorKind::CannotOpen:
        return KIO::WorkerResult::fail(KIO::ERR_CANNOT_OPEN_FOR_READING, subject);
    case ErrorKind::ReadFailed:
        return KIO::WorkerResult::fail(KIO::ERR_CANNOT_READ, subject);
    case ErrorKind::Unsupported:
        return KIO::WorkerResult::fail(KIO::ERR_WORKER_DEFINED, error.message());
    case ErrorKind::IsFile:
        return KIO::WorkerResult::fail(KIO::ERR_IS_FILE, subject);
    case ErrorKind::IsDirectory:
        return KIO::WorkerResult::fail(KIO::ERR_IS_DIRECTORY, subject);
    case ErrorKind::Cancelled:
        return KIO::WorkerResult::fail(KIO::ERR_USER_CANCELED, subject);
    }
    return KIO::WorkerResult::fail(KIO::ERR_WORKER_DEFINED, error.message());
}

ResolvedPath UnpackWorker::resolve(const QUrl &url)
{
    return m_resolver.resolve(url.path(), [this] {
        return wasKilled();
    });
}

KIO::UDSEntry UnpackWorker::imageRootEntry(const ResolvedPath &resolved, const QString &name) const
{
    // REQ-023: a directory with the image's MIME type, size and time.
    const QFileInfo info(resolved.imagePath);
    const EntryTable &table = resolved.volume->entries();
    KIO::UDSEntry entry;
    entry.fastInsert(KIO::UDSEntry::UDS_NAME, name);
    entry.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME, info.fileName());
    entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, table.permissions(EntryTable::root()) & kDirectoryAccessMask);
    entry.fastInsert(KIO::UDSEntry::UDS_SIZE, info.size());
    entry.fastInsert(KIO::UDSEntry::UDS_MODIFICATION_TIME, info.lastModified().toSecsSinceEpoch());
    entry.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, QMimeDatabase().mimeTypeForFile(resolved.imagePath).name());
    return entry;
}

KIO::UDSEntry UnpackWorker::entryFor(const EntryTable &table, EntryTable::Index index, const QString &name) const
{
    // REQ-021, REQ-024, REQ-026; docs/architecture.md, section 10.
    const EntryType type = table.type(index);
    KIO::UDSEntry entry;
    entry.fastInsert(KIO::UDSEntry::UDS_NAME, name);
    entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, fileTypeOf(type));
    std::uint32_t access = table.permissions(index);
    if (type == EntryType::Directory) {
        access &= kDirectoryAccessMask;
        entry.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, QStringLiteral("inode/directory"));
    }
    entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, access);
    if (type == EntryType::File || type == EntryType::Other) {
        entry.fastInsert(KIO::UDSEntry::UDS_SIZE, static_cast<long long>(table.size(index)));
    }
    if (const auto mtime = table.mtime(index)) {
        entry.fastInsert(KIO::UDSEntry::UDS_MODIFICATION_TIME, *mtime);
    }
    if (type == EntryType::Symlink) {
        entry.fastInsert(KIO::UDSEntry::UDS_LINK_DEST, QString::fromUtf8(table.linkTarget(index)));
    }
    // Owners only when the format records them; as numbers, because the
    // image's IDs mean nothing on this system.
    if (const auto uid = table.uid(index)) {
        entry.fastInsert(KIO::UDSEntry::UDS_USER, QString::number(*uid));
    }
    if (const auto gid = table.gid(index)) {
        entry.fastInsert(KIO::UDSEntry::UDS_GROUP, QString::number(*gid));
    }
    return entry;
}

QString UnpackWorker::sniffMimeType(const ResolvedPath &resolved, const QString &name)
{
    QByteArray head;
    resolved.volume->extract(
        resolved.entry,
        [&head](const char *data, std::size_t length) {
            const qsizetype take = std::min<qsizetype>(kMimeSniffSize - head.size(), static_cast<qsizetype>(length));
            head.append(data, take);
            return head.size() < kMimeSniffSize;
        },
        [this] {
            return wasKilled();
        });
    return QMimeDatabase().mimeTypeForFileNameAndData(name, head).name();
}

KIO::WorkerResult UnpackWorker::listDir(const QUrl &url)
{
    return guard(url, [&] {
        const ResolvedPath resolved = resolve(url);
        const EntryTable &table = resolved.volume->entries();
        if (table.type(resolved.entry) != EntryType::Directory) {
            return KIO::WorkerResult::fail(KIO::ERR_IS_FILE, url.toDisplayString());
        }

        // REQ-022: exactly one "." entry describing the listed directory.
        listEntry(resolved.isImageRoot() ? imageRootEntry(resolved, QStringLiteral(".")) : entryFor(table, resolved.entry, QStringLiteral(".")));

        const EntryTable::Index first = table.firstChild(resolved.entry);
        const EntryTable::Index count = table.childCount(resolved.entry);
        for (EntryTable::Index i = first; i < first + count; ++i) {
            if (wasKilled()) {
                return KIO::WorkerResult::fail(KIO::ERR_USER_CANCELED, url.toDisplayString());
            }
            listEntry(entryFor(table, i, nameOf(table, i)));
        }
        return KIO::WorkerResult::pass();
    });
}

KIO::WorkerResult UnpackWorker::stat(const QUrl &url)
{
    return guard(url, [&] {
        const ResolvedPath resolved = resolve(url);
        if (resolved.isImageRoot()) {
            statEntry(imageRootEntry(resolved, QFileInfo(resolved.imagePath).fileName()));
            return KIO::WorkerResult::pass();
        }

        const EntryTable &table = resolved.volume->entries();
        const QString name = nameOf(table, resolved.entry);
        KIO::UDSEntry entry = entryFor(table, resolved.entry, name);

        // REQ-029: the MIME type of a regular file from its name and first bytes.
        const QString detailsValue = metaData(QStringLiteral("details"));
        const KIO::StatDetails details = detailsValue.isEmpty() ? KIO::StatDefaultDetails : static_cast<KIO::StatDetails>(detailsValue.toInt());
        if ((details & KIO::StatMimeType) && table.type(resolved.entry) == EntryType::File) {
            entry.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, sniffMimeType(resolved, name));
        }
        statEntry(entry);
        return KIO::WorkerResult::pass();
    });
}

KIO::WorkerResult UnpackWorker::get(const QUrl &url)
{
    return guard(url, [&] {
        const ResolvedPath resolved = resolve(url);
        const EntryTable &table = resolved.volume->entries();
        const EntryType type = table.type(resolved.entry);
        if (resolved.isImageRoot() || type == EntryType::Directory) {
            return KIO::WorkerResult::fail(KIO::ERR_IS_DIRECTORY, url.toDisplayString());
        }
        if (type == EntryType::Symlink) {
            // M0b redirects to the link target (REQ-032, REQ-033).
            return KIO::WorkerResult::fail(KIO::ERR_CANNOT_OPEN_FOR_READING, url.toDisplayString());
        }

        // REQ-030: total size first, the MIME type once, then the bytes in
        // fixed-size chunks, ending with an empty data().
        const QString name = nameOf(table, resolved.entry);
        totalSize(table.size(resolved.entry));

        QMimeDatabase mimeDatabase;
        QByteArray chunk;
        chunk.reserve(kDataChunkSize);
        bool mimeTypeSent = false;
        KIO::filesize_t processed = 0;
        auto flush = [&] {
            if (!mimeTypeSent) {
                mimeType(mimeDatabase.mimeTypeForFileNameAndData(name, chunk).name());
                mimeTypeSent = true;
            }
            data(chunk);
            processed += static_cast<KIO::filesize_t>(chunk.size());
            processedSize(processed);
            chunk.truncate(0);
        };

        const ExtractResult result = resolved.volume->extract(
            resolved.entry,
            [&](const char *bytes, std::size_t length) {
                while (length > 0) {
                    const qsizetype take = std::min<qsizetype>(kDataChunkSize - chunk.size(), static_cast<qsizetype>(length));
                    chunk.append(bytes, take);
                    bytes += take;
                    length -= static_cast<std::size_t>(take);
                    if (chunk.size() == kDataChunkSize) {
                        flush();
                        if (wasKilled()) {
                            return false;
                        }
                    }
                }
                return true;
            },
            [this] {
                return wasKilled();
            });

        if (result == ExtractResult::Stopped || wasKilled()) {
            return KIO::WorkerResult::fail(KIO::ERR_USER_CANCELED, url.toDisplayString());
        }
        if (!chunk.isEmpty()) {
            flush();
        }
        if (!mimeTypeSent) {
            mimeType(mimeDatabase.mimeTypeForFile(name, QMimeDatabase::MatchExtension).name());
        }
        data(QByteArray());
        return KIO::WorkerResult::pass();
    });
}
