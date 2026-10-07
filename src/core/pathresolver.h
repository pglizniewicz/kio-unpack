/*
    SPDX-License-Identifier: GPL-2.0-or-later
    SPDX-FileCopyrightText: 2026 kio-unpack contributors
*/

#pragma once

#include "volume.h"

#include <QString>

#include <memory>

namespace KioUnpack
{

// Opens the image at an on-disk path. Throws Error.
using ImageOpener = std::function<std::unique_ptr<Volume>(const QString &imagePath, const StopPredicate &shouldStop)>;

// The opener used outside tests: FileByteSource, Sniff, then the backend for
// the signature found. M2 replaces the direct call with the backend registry.
ImageOpener defaultImageOpener();

struct ResolvedPath {
    QString imagePath; // absolute path of the image on disk
    QString innerPath; // path inside the image, without leading or trailing '/'; empty for the root
    std::shared_ptr<Volume> volume;
    EntryTable::Index entry = EntryTable::root();

    bool isImageRoot() const
    {
        return innerPath.isEmpty();
    }
};

/*
 * Splits an unpack: path into the image on disk and the path inside it
 * (docs/architecture.md, section 6, the M0a steps): normalise, walk the
 * on-disk prefixes from the shortest, open the first one that is not a
 * directory, then walk the remaining components inside the image. Symbolic
 * links inside the image are never followed. Every failure throws Error.
 */
class PathResolver
{
public:
    explicit PathResolver(ImageOpener opener = defaultImageOpener());

    ResolvedPath resolve(const QString &path, const StopPredicate &shouldStop = {}) const;

    // Collapses repeated '/' and drops a trailing '/' (REQ-011).
    static QString normalise(const QString &path);

private:
    ImageOpener m_opener;
};

} // namespace KioUnpack
