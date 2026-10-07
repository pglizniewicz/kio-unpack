/*
    SPDX-License-Identifier: GPL-2.0-or-later
    SPDX-FileCopyrightText: 2026 kio-unpack contributors
*/

#pragma once

#include "core/bytesource.h"
#include "core/volume.h"

#include <memory>

namespace KioUnpack
{

/*
 * An ISO 9660 volume read through libarchive (docs/architecture.md, section
 * 4.4). The archive is read through callbacks over a ByteSource. One header
 * pass builds the entry table; the handle of an entry is its header ordinal.
 * Extraction reopens the archive and walks the headers up to that ordinal.
 *
 * Names follow libarchive's own choice: Rock Ridge when present, otherwise
 * Joliet, otherwise plain ISO 9660. Without Rock Ridge the format has no
 * permissions or owners; the entry table then synthesises modes.
 */
class LibarchiveVolume : public Volume
{
public:
    // Throws Error: Unsupported when libarchive cannot read the data,
    // Cancelled when `shouldStop` fires, ReadFailed when the source fails.
    static std::unique_ptr<Volume> open(std::unique_ptr<ByteSource> source, const StopPredicate &shouldStop);

    const EntryTable &entries() const override;
    ExtractResult extract(EntryTable::Index entry, const DataSink &sink, const StopPredicate &shouldStop) override;

private:
    explicit LibarchiveVolume(std::unique_ptr<ByteSource> source);

    std::unique_ptr<ByteSource> m_source;
    EntryTable m_entries;
};

} // namespace KioUnpack
