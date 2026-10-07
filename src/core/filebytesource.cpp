/*
    SPDX-License-Identifier: GPL-2.0-or-later
    SPDX-FileCopyrightText: 2026 kio-unpack contributors
*/

#include "filebytesource.h"

#include "error.h"

#include <KLocalizedString>

#include <QFile>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace KioUnpack
{

ByteSource::~ByteSource() = default;

FileByteSource::FileByteSource(const QString &path)
    : m_path(path)
{
    const QByteArray encoded = QFile::encodeName(path);
    // O_NONBLOCK: opening a FIFO must not wait for a writer; the type check
    // below rejects it before anything is read.
    m_fd = ::open(encoded.constData(), O_RDONLY | O_CLOEXEC | O_NOCTTY | O_NONBLOCK);
    if (m_fd < 0) {
        throw Error::fromErrno(errno, path);
    }

    struct stat st{};
    if (::fstat(m_fd, &st) != 0) {
        const int savedErrno = errno;
        ::close(m_fd);
        m_fd = -1;
        throw Error::fromErrno(savedErrno, path);
    }
    if (!S_ISREG(st.st_mode)) {
        ::close(m_fd);
        m_fd = -1;
        throw Error::cannotOpen(path, i18n("Not a regular file."));
    }

    m_identity.device = st.st_dev;
    m_identity.inode = st.st_ino;
    m_identity.size = static_cast<std::uint64_t>(st.st_size);
    m_identity.mtimeSeconds = st.st_mtim.tv_sec;
    m_identity.mtimeNanoseconds = st.st_mtim.tv_nsec;
}

FileByteSource::~FileByteSource()
{
    if (m_fd >= 0) {
        ::close(m_fd);
    }
}

std::uint64_t FileByteSource::size() const
{
    return m_identity.size;
}

std::size_t FileByteSource::readAt(std::uint64_t offset, void *buffer, std::size_t length)
{
    auto *out = static_cast<char *>(buffer);
    std::size_t done = 0;
    while (done < length) {
        const ssize_t n = ::pread(m_fd, out + done, length - done, static_cast<off_t>(offset + done));
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw Error::readFailed(m_path, QString::fromLocal8Bit(std::strerror(errno)));
        }
        if (n == 0) {
            break; // end of file
        }
        done += static_cast<std::size_t>(n);
    }
    return done;
}

QString FileByteSource::describe() const
{
    return m_path;
}

} // namespace KioUnpack
