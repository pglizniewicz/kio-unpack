/*
    SPDX-License-Identifier: GPL-2.0-or-later
    SPDX-FileCopyrightText: 2026 kio-unpack contributors
*/

#include "libarchivevolume.h"

#include "core/error.h"

#include <KLocalizedString>

#include <QHash>

#include <archive.h>
#include <archive_entry.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <exception>
#include <vector>

namespace KioUnpack
{

namespace
{

// Size of each read that libarchive requests from the ByteSource.
constexpr std::size_t kReadBlockSize = 64 * 1024;
// Size of the zero buffer used to fill holes in sparse entries.
constexpr std::size_t kHoleFillBlockSize = 64 * 1024;

/*
 * One libarchive read handle over a ByteSource. Exceptions from the source
 * must not cross libarchive's C frames, so the callbacks store them and the
 * caller rethrows after libarchive returns.
 */
class Reader
{
public:
    explicit Reader(ByteSource &source)
        : m_source(source)
        , m_archive(archive_read_new())
        , m_buffer(kReadBlockSize)
    {
        if (!m_archive) {
            throw std::bad_alloc();
        }
        archive_read_support_format_iso9660(m_archive);
        archive_read_set_read_callback(m_archive, &Reader::readCallback);
        archive_read_set_seek_callback(m_archive, &Reader::seekCallback);
        archive_read_set_skip_callback(m_archive, &Reader::skipCallback);
        archive_read_set_callback_data(m_archive, this);
    }

    ~Reader()
    {
        archive_read_free(m_archive);
    }

    Reader(const Reader &) = delete;
    Reader &operator=(const Reader &) = delete;

    archive *handle() const
    {
        return m_archive;
    }

    // Rethrows an exception a callback stored, if any.
    void rethrowPending()
    {
        if (m_pending) {
            std::exception_ptr pending = m_pending;
            m_pending = nullptr;
            std::rethrow_exception(pending);
        }
    }

    QString errorString() const
    {
        const char *message = archive_error_string(m_archive);
        return message ? QString::fromLocal8Bit(message) : i18n("Unknown error.");
    }

private:
    static la_ssize_t readCallback(archive *a, void *clientData, const void **buffer)
    {
        auto *self = static_cast<Reader *>(clientData);
        try {
            const std::size_t n = self->m_source.readAt(self->m_position, self->m_buffer.data(), self->m_buffer.size());
            self->m_position += n;
            *buffer = self->m_buffer.data();
            return static_cast<la_ssize_t>(n);
        } catch (...) {
            self->m_pending = std::current_exception();
            archive_set_error(a, EIO, "read failed");
            return ARCHIVE_FATAL;
        }
    }

    static la_int64_t seekCallback(archive *, void *clientData, la_int64_t offset, int whence)
    {
        auto *self = static_cast<Reader *>(clientData);
        const auto size = static_cast<la_int64_t>(self->m_source.size());
        la_int64_t base = 0;
        switch (whence) {
        case SEEK_SET:
            base = 0;
            break;
        case SEEK_CUR:
            base = static_cast<la_int64_t>(self->m_position);
            break;
        case SEEK_END:
            base = size;
            break;
        default:
            return ARCHIVE_FATAL;
        }
        const la_int64_t target = base + offset;
        if (target < 0) {
            return ARCHIVE_FATAL;
        }
        self->m_position = static_cast<std::uint64_t>(target);
        return target;
    }

    static la_int64_t skipCallback(archive *, void *clientData, la_int64_t request)
    {
        auto *self = static_cast<Reader *>(clientData);
        if (request <= 0) {
            return 0;
        }
        const std::uint64_t size = self->m_source.size();
        const std::uint64_t remaining = self->m_position < size ? size - self->m_position : 0;
        const auto skipped = std::min<std::uint64_t>(static_cast<std::uint64_t>(request), remaining);
        self->m_position += skipped;
        return static_cast<la_int64_t>(skipped);
    }

    ByteSource &m_source;
    archive *m_archive;
    std::vector<char> m_buffer;
    std::uint64_t m_position = 0;
    std::exception_ptr m_pending;
};

void openReader(Reader &reader, const ByteSource &source)
{
    const int r = archive_read_open1(reader.handle());
    reader.rethrowPending();
    if (r != ARCHIVE_OK) {
        throw Error::unsupported(source.describe());
    }
}

EntryType entryTypeOf(archive_entry *entry)
{
    switch (archive_entry_filetype(entry)) {
    case AE_IFDIR:
        return EntryType::Directory;
    case AE_IFREG:
        return EntryType::File;
    case AE_IFLNK:
        return EntryType::Symlink;
    default:
        return EntryType::Other;
    }
}

bool stopRequested(const StopPredicate &shouldStop)
{
    return shouldStop && shouldStop();
}

} // namespace

LibarchiveVolume::LibarchiveVolume(std::unique_ptr<ByteSource> source)
    : m_source(std::move(source))
{
}

std::unique_ptr<Volume> LibarchiveVolume::open(std::unique_ptr<ByteSource> source, const StopPredicate &shouldStop)
{
    std::unique_ptr<LibarchiveVolume> volume(new LibarchiveVolume(std::move(source)));

    Reader reader(*volume->m_source);
    openReader(reader, *volume->m_source);

    EntryTable::Builder builder;
    // Regular files by path, so that hard links can share their data.
    QHash<QByteArray, EntryTable::Attributes> filesByPath;
    std::uint64_t ordinal = 0;
    bool firstHeader = true;
    bool rockRidge = false;

    for (;; ++ordinal) {
        if (stopRequested(shouldStop)) {
            throw Error::cancelled();
        }
        archive_entry *entry = nullptr;
        const int r = archive_read_next_header(reader.handle(), &entry);
        reader.rethrowPending();
        if (r == ARCHIVE_EOF) {
            break;
        }
        if (r < ARCHIVE_WARN) {
            throw Error::unsupported(volume->m_source->describe());
        }
        if (firstHeader) {
            // archive_format() reports Rock Ridge only when it was used
            // (docs/architecture.md, section 4.4); without it the format has
            // no permissions or owners.
            rockRidge = archive_format(reader.handle()) == ARCHIVE_FORMAT_ISO9660_ROCKRIDGE;
            firstHeader = false;
        }

        const char *pathname = archive_entry_pathname(entry);
        if (pathname) {
            const QByteArray path(pathname);
            EntryTable::Attributes attributes;
            attributes.handle = ordinal;
            attributes.type = entryTypeOf(entry);
            if (archive_entry_size_is_set(entry) && archive_entry_size(entry) > 0) {
                attributes.size = static_cast<std::uint64_t>(archive_entry_size(entry));
            }
            if (archive_entry_mtime_is_set(entry)) {
                attributes.mtime = archive_entry_mtime(entry);
            }
            if (rockRidge) {
                attributes.mode = static_cast<std::uint32_t>(archive_entry_perm(entry));
                attributes.uid = static_cast<std::uint32_t>(archive_entry_uid(entry));
                attributes.gid = static_cast<std::uint32_t>(archive_entry_gid(entry));
            }
            if (attributes.type == EntryType::Symlink) {
                if (const char *target = archive_entry_symlink(entry)) {
                    attributes.linkTarget = QByteArray(target);
                }
            }
            if (const char *hardlink = archive_entry_hardlink(entry)) {
                // A hard link carries no data of its own; it reads the data
                // of the entry it names.
                const auto target = filesByPath.constFind(QByteArray(hardlink));
                if (target != filesByPath.constEnd()) {
                    attributes.type = EntryType::File;
                    attributes.size = target->size;
                    attributes.handle = target->handle;
                }
            }
            if (attributes.type == EntryType::File) {
                filesByPath.insert(path, attributes);
            }
            builder.add(std::string_view(path.constData(), static_cast<std::size_t>(path.size())), attributes);
        }

        const int skipResult = archive_read_data_skip(reader.handle());
        reader.rethrowPending();
        if (skipResult < ARCHIVE_WARN) {
            throw Error::unsupported(volume->m_source->describe());
        }
    }

    volume->m_entries = builder.finish();
    return volume;
}

const EntryTable &LibarchiveVolume::entries() const
{
    return m_entries;
}

ExtractResult LibarchiveVolume::extract(EntryTable::Index index, const DataSink &sink, const StopPredicate &shouldStop)
{
    const std::uint64_t wanted = m_entries.handle(index);
    const QString subject = m_source->describe() + u'/' + QString::fromUtf8(m_entries.name(index));

    Reader reader(*m_source);
    openReader(reader, *m_source);

    // Walk the headers up to the entry; the walk reads no file data.
    for (std::uint64_t ordinal = 0;; ++ordinal) {
        if (stopRequested(shouldStop)) {
            return ExtractResult::Stopped;
        }
        archive_entry *entry = nullptr;
        const int r = archive_read_next_header(reader.handle(), &entry);
        reader.rethrowPending();
        if (r == ARCHIVE_EOF) {
            throw Error::readFailed(subject, i18n("The entry was not found when reading the image again."));
        }
        if (r < ARCHIVE_WARN) {
            throw Error::readFailed(subject, reader.errorString());
        }
        if (ordinal == wanted) {
            break;
        }
        const int skipResult = archive_read_data_skip(reader.handle());
        reader.rethrowPending();
        if (skipResult < ARCHIVE_WARN) {
            throw Error::readFailed(subject, reader.errorString());
        }
    }

    // Stream the data. libarchive reports sparse regions as gaps between
    // block offsets; they are delivered as zeros.
    static const std::vector<char> zeros(kHoleFillBlockSize, 0);
    std::uint64_t delivered = 0;
    for (;;) {
        if (stopRequested(shouldStop)) {
            return ExtractResult::Stopped;
        }
        const void *block = nullptr;
        std::size_t length = 0;
        la_int64_t offset = 0;
        const int r = archive_read_data_block(reader.handle(), &block, &length, &offset);
        reader.rethrowPending();
        if (r == ARCHIVE_EOF) {
            return ExtractResult::Completed;
        }
        if (r < ARCHIVE_WARN) {
            throw Error::readFailed(subject, reader.errorString());
        }
        while (offset >= 0 && delivered < static_cast<std::uint64_t>(offset)) {
            const auto gap = std::min<std::uint64_t>(static_cast<std::uint64_t>(offset) - delivered, zeros.size());
            if (!sink(zeros.data(), static_cast<std::size_t>(gap))) {
                return ExtractResult::Stopped;
            }
            delivered += gap;
        }
        if (length > 0) {
            if (!sink(static_cast<const char *>(block), length)) {
                return ExtractResult::Stopped;
            }
            delivered += length;
        }
    }
}

} // namespace KioUnpack
