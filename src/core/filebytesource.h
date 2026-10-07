/*
    SPDX-License-Identifier: GPL-2.0-or-later
    SPDX-FileCopyrightText: 2026 kio-unpack contributors
*/

#pragma once

#include "bytesource.h"

#include <sys/types.h>

namespace KioUnpack
{

// The identity of an opened file; a session is valid while it is unchanged
// (docs/architecture.md, section 9).
struct FileIdentity {
    dev_t device = 0;
    ino_t inode = 0;
    std::uint64_t size = 0;
    std::int64_t mtimeSeconds = 0;
    std::int64_t mtimeNanoseconds = 0;
};

// A regular file on disk, opened read-only.
class FileByteSource : public ByteSource
{
public:
    // Throws Error: NotFound, AccessDenied, or CannotOpen, which includes paths
    // that are not regular files (directories, FIFOs, devices).
    explicit FileByteSource(const QString &path);
    ~FileByteSource() override;

    std::uint64_t size() const override;
    std::size_t readAt(std::uint64_t offset, void *buffer, std::size_t length) override;
    QString describe() const override;

    const FileIdentity &identity() const
    {
        return m_identity;
    }

private:
    QString m_path;
    int m_fd = -1;
    FileIdentity m_identity;
};

} // namespace KioUnpack
