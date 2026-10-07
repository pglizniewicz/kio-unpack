/*
    SPDX-License-Identifier: GPL-2.0-or-later
    SPDX-FileCopyrightText: 2026 kio-unpack contributors
*/

#include "sniff.h"

#include "bytesource.h"

#include <array>
#include <cstring>

namespace KioUnpack
{

namespace
{
// ECMA-119: the volume descriptor set starts at logical sector 16 of 2048
// bytes; each descriptor has a type byte followed by the identifier "CD001".
constexpr std::uint64_t kIsoDescriptorOffset = 16 * 2048;
constexpr std::size_t kIsoIdentifierOffset = 1;
constexpr char kIsoIdentifier[] = "CD001";
constexpr std::size_t kIsoIdentifierLength = sizeof(kIsoIdentifier) - 1;
}

SniffResult sniff(ByteSource &source)
{
    SniffResult result;

    std::array<char, kIsoIdentifierOffset + kIsoIdentifierLength> descriptor{};
    if (source.readAt(kIsoDescriptorOffset, descriptor.data(), descriptor.size()) == descriptor.size()
        && std::memcmp(descriptor.data() + kIsoIdentifierOffset, kIsoIdentifier, kIsoIdentifierLength) == 0) {
        result.signatures.append(Signature::Iso9660);
    }

    return result;
}

} // namespace KioUnpack
