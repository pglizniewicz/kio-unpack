/*
    SPDX-License-Identifier: GPL-2.0-or-later
    SPDX-FileCopyrightText: 2026 kio-unpack contributors
*/

#pragma once

#include <QList>

namespace KioUnpack
{

class ByteSource;

// Signatures Sniff can recognise. M0a knows ISO 9660 only; later stages add
// the rest of docs/architecture.md, section 4.5.
enum class Signature {
    Iso9660, // "CD001" in the first volume descriptor
};

struct SniffResult {
    QList<Signature> signatures;

    bool has(Signature signature) const
    {
        return signatures.contains(signature);
    }
};

// Reads a few fixed regions of the source once and records the signatures
// found. Throws Error (ReadFailed) when the source cannot be read.
SniffResult sniff(ByteSource &source);

} // namespace KioUnpack
