/*
    SPDX-License-Identifier: GPL-2.0-or-later
    SPDX-FileCopyrightText: 2026 kio-unpack contributors
*/

#pragma once

#include <QByteArray>
#include <QString>

#include <exception>

namespace KioUnpack
{

/*
 * What went wrong, in the terms of the error table in docs/architecture.md,
 * section 10. The worker maps each kind to one KIO error code; the core knows
 * nothing about KIO.
 */
enum class ErrorKind {
    NotFound, // a path on disk or inside the image does not exist
    AccessDenied, // EACCES or EPERM on disk
    CannotOpen, // the image cannot be opened for reading
    ReadFailed, // reading data failed, or the data is corrupt or truncated
    Unsupported, // no reader accepts the data
    IsFile, // the path continues below a file
    IsDirectory, // a file was expected, a directory was found
    Cancelled, // the client cancelled the operation
};

/*
 * The single exception type the core throws. `subject` is the path or name the
 * error is about; `message` is a complete, translated sentence for clients that
 * show free text (ERR_WORKER_DEFINED, the command-line tool).
 */
class Error : public std::exception
{
public:
    Error(ErrorKind kind, const QString &subject, const QString &message);

    ErrorKind kind() const noexcept
    {
        return m_kind;
    }
    const QString &subject() const noexcept
    {
        return m_subject;
    }
    const QString &message() const noexcept
    {
        return m_message;
    }
    const char *what() const noexcept override;

    // Factories with the standard message for each kind.
    static Error notFound(const QString &subject);
    static Error accessDenied(const QString &subject);
    static Error cannotOpen(const QString &subject, const QString &reason);
    static Error readFailed(const QString &subject, const QString &reason);
    static Error unsupported(const QString &subject);
    static Error isFile(const QString &subject);
    static Error isDirectory(const QString &subject);
    static Error cancelled();

    // Maps an errno from an operation on a path on disk to an error.
    static Error fromErrno(int errorNumber, const QString &subject);

private:
    ErrorKind m_kind;
    QString m_subject;
    QString m_message;
    QByteArray m_what;
};

} // namespace KioUnpack
