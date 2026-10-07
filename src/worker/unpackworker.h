/*
    SPDX-License-Identifier: GPL-2.0-or-later
    SPDX-FileCopyrightText: 2026 kio-unpack contributors
*/

#pragma once

#include "core/pathresolver.h"

#include <KIO/UDSEntry>
#include <KIO/WorkerBase>

namespace KioUnpack
{
class Error;
}

/*
 * The unpack: worker: a thin adapter that maps URLs to resolver calls and
 * results to UDS entries and data (docs/architecture.md, section 3). Write
 * operations keep WorkerBase's default ERR_UNSUPPORTED_ACTION (REQ-035).
 */
class UnpackWorker : public KIO::WorkerBase
{
public:
    UnpackWorker(const QByteArray &poolSocket, const QByteArray &appSocket);

    KIO::WorkerResult listDir(const QUrl &url) override;
    KIO::WorkerResult stat(const QUrl &url) override;
    KIO::WorkerResult get(const QUrl &url) override;

private:
    // Runs one operation and turns every exception into a KIO error
    // (contract 3, docs/architecture.md, section 4). The full guard() with
    // the watchdog comes in M0b and M0c.
    template<typename Operation>
    KIO::WorkerResult guard(const QUrl &url, Operation &&operation);
    static KIO::WorkerResult fail(const QUrl &url, const KioUnpack::Error &error);

    KioUnpack::ResolvedPath resolve(const QUrl &url);
    KIO::UDSEntry imageRootEntry(const KioUnpack::ResolvedPath &resolved, const QString &name) const;
    KIO::UDSEntry entryFor(const KioUnpack::EntryTable &table, KioUnpack::EntryTable::Index index, const QString &name) const;
    QString sniffMimeType(const KioUnpack::ResolvedPath &resolved, const QString &name);

    KioUnpack::PathResolver m_resolver;
};
