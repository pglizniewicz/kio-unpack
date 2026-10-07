/*
    SPDX-License-Identifier: GPL-2.0-or-later
    SPDX-FileCopyrightText: 2026 kio-unpack contributors
*/

#include "error.h"

#include <KLocalizedString>

#include <cerrno>
#include <cstring>

namespace KioUnpack
{

Error::Error(ErrorKind kind, const QString &subject, const QString &message)
    : m_kind(kind)
    , m_subject(subject)
    , m_message(message)
    , m_what(message.toUtf8())
{
}

const char *Error::what() const noexcept
{
    return m_what.constData();
}

Error Error::notFound(const QString &subject)
{
    return Error(ErrorKind::NotFound, subject, i18n("The file or folder %1 does not exist.", subject));
}

Error Error::accessDenied(const QString &subject)
{
    return Error(ErrorKind::AccessDenied, subject, i18n("Access to %1 was denied.", subject));
}

Error Error::cannotOpen(const QString &subject, const QString &reason)
{
    return Error(ErrorKind::CannotOpen, subject, i18n("Could not open %1 for reading: %2", subject, reason));
}

Error Error::readFailed(const QString &subject, const QString &reason)
{
    return Error(ErrorKind::ReadFailed, subject, i18n("Could not read %1: %2", subject, reason));
}

Error Error::unsupported(const QString &subject)
{
    return Error(ErrorKind::Unsupported, subject, i18n("%1 is not a supported disc image or archive, or its data is damaged.", subject));
}

Error Error::isFile(const QString &subject)
{
    return Error(ErrorKind::IsFile, subject, i18n("%1 is a file, not a folder.", subject));
}

Error Error::isDirectory(const QString &subject)
{
    return Error(ErrorKind::IsDirectory, subject, i18n("%1 is a folder, not a file.", subject));
}

Error Error::cancelled()
{
    return Error(ErrorKind::Cancelled, QString(), i18n("The operation was cancelled."));
}

Error Error::fromErrno(int errorNumber, const QString &subject)
{
    switch (errorNumber) {
    case ENOENT:
    case ENOTDIR:
        return notFound(subject);
    case EACCES:
    case EPERM:
        return accessDenied(subject);
    default:
        return cannotOpen(subject, QString::fromLocal8Bit(std::strerror(errorNumber)));
    }
}

} // namespace KioUnpack
