/*
    SPDX-License-Identifier: GPL-2.0-or-later
    SPDX-FileCopyrightText: 2026 kio-unpack contributors
*/

#include "pathresolver.h"

#include "error.h"
#include "filebytesource.h"
#include "sniff.h"

#include "backends/libarchive/libarchivevolume.h"

#include <KLocalizedString>

#include <QFile>
#include <QStringList>

#include <cerrno>
#include <sys/stat.h>

namespace KioUnpack
{

Volume::~Volume() = default;

ImageOpener defaultImageOpener()
{
    return [](const QString &imagePath, const StopPredicate &shouldStop) -> std::unique_ptr<Volume> {
        auto source = std::make_unique<FileByteSource>(imagePath);
        const SniffResult signatures = sniff(*source);
        if (signatures.has(Signature::Iso9660)) {
            return LibarchiveVolume::open(std::move(source), shouldStop);
        }
        throw Error::unsupported(imagePath);
    };
}

PathResolver::PathResolver(ImageOpener opener)
    : m_opener(std::move(opener))
{
}

QString PathResolver::normalise(const QString &path)
{
    QString result;
    result.reserve(path.size());
    for (const QChar c : path) {
        if (c == u'/' && result.endsWith(u'/')) {
            continue;
        }
        result.append(c);
    }
    if (result.size() > 1 && result.endsWith(u'/')) {
        result.chop(1);
    }
    return result;
}

ResolvedPath PathResolver::resolve(const QString &path, const StopPredicate &shouldStop) const
{
    const QString normalised = normalise(path);
    if (!normalised.startsWith(u'/')) {
        throw Error::notFound(path);
    }

    const QStringList components = normalised.split(u'/', Qt::SkipEmptyParts);

    // Stat-walk on disk: directories continue the walk, the first prefix that
    // is not a directory is the image (REQ-009). Symlinks on disk are followed.
    QString prefix;
    qsizetype imageComponents = -1;
    for (qsizetype i = 0; i < components.size(); ++i) {
        prefix += u'/' + components.at(i);
        struct stat st{};
        if (::stat(QFile::encodeName(prefix).constData(), &st) != 0) {
            throw Error::fromErrno(errno, prefix);
        }
        if (!S_ISDIR(st.st_mode)) {
            imageComponents = i + 1;
            break;
        }
    }
    if (imageComponents < 0) {
        // The whole path is a directory on disk. M0b redirects to file:
        // (REQ-010); until then it is not something this worker can open.
        throw Error::unsupported(normalised);
    }

    ResolvedPath result;
    result.imagePath = prefix;
    result.innerPath = components.mid(imageComponents).join(u'/');
    result.volume = m_opener(prefix, shouldStop);

    // Inner walk: exact lookup, directories descend, nothing is followed.
    const EntryTable &table = result.volume->entries();
    EntryTable::Index current = EntryTable::root();
    QString walked = prefix;
    for (qsizetype i = imageComponents; i < components.size(); ++i) {
        const EntryType currentType = table.type(current);
        if (currentType == EntryType::File || currentType == EntryType::Other) {
            throw Error::isFile(walked);
        }
        if (currentType != EntryType::Directory) {
            throw Error::notFound(normalised); // below a symbolic link
        }
        const QByteArray name = components.at(i).toUtf8();
        const auto child = table.child(current, std::string_view(name.constData(), static_cast<std::size_t>(name.size())));
        if (!child) {
            throw Error::notFound(normalised);
        }
        current = *child;
        walked += u'/' + components.at(i);
    }
    result.entry = current;
    return result;
}

} // namespace KioUnpack
