/*
    SPDX-License-Identifier: GPL-2.0-or-later
    SPDX-FileCopyrightText: 2026 kio-unpack contributors
*/

#pragma once

#include <QString>

#include <cstddef>
#include <cstdint>

namespace KioUnpack
{

/*
 * Contract 1 of docs/architecture.md, section 4: read-only, seekable random
 * access to bytes of known size. A read returns fewer bytes than requested only
 * at the end of the data; a failed read throws Error (ReadFailed).
 */
class ByteSource
{
public:
    ByteSource() = default;
    virtual ~ByteSource();

    ByteSource(const ByteSource &) = delete;
    ByteSource &operator=(const ByteSource &) = delete;

    virtual std::uint64_t size() const = 0;
    virtual std::size_t readAt(std::uint64_t offset, void *buffer, std::size_t length) = 0;
    // For log lines and error messages.
    virtual QString describe() const = 0;
};

} // namespace KioUnpack
