/*
    SPDX-License-Identifier: GPL-2.0-or-later
    SPDX-FileCopyrightText: 2026 kio-unpack contributors
*/

#pragma once

#include "entrytable.h"

#include <cstddef>
#include <functional>

namespace KioUnpack
{

// Receives an entry's bytes in order; returning false stops the transfer.
using DataSink = std::function<bool(const char *data, std::size_t length)>;
// Polled in every loop over entries or data blocks; true means "stop now"
// (the worker passes wasKilled()).
using StopPredicate = std::function<bool()>;

enum class ExtractResult {
    Completed,
    Stopped, // the sink returned false or the stop predicate fired
};

// One opened volume (docs/architecture.md, section 4.3), used from one thread.
class Volume
{
public:
    Volume() = default;
    virtual ~Volume();

    Volume(const Volume &) = delete;
    Volume &operator=(const Volume &) = delete;

    virtual const EntryTable &entries() const = 0;

    // Pushes the bytes of a regular file to `sink`. Throws Error (ReadFailed)
    // when the data cannot be read.
    virtual ExtractResult extract(EntryTable::Index entry, const DataSink &sink, const StopPredicate &shouldStop) = 0;
};

} // namespace KioUnpack
