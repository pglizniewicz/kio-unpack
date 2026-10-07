/*
    SPDX-License-Identifier: GPL-2.0-or-later
    SPDX-FileCopyrightText: 2026 kio-unpack contributors
*/

#include "unpackworker.h"

#include <KIO/WorkerFactory>

#include <QCoreApplication>

#include <cstdio>
#include <memory>

// Carries the plugin metadata that KIO reads to register the protocol
// (REQ-001, REQ-002); the worker process itself starts through kdemain.
class UnpackWorkerFactory : public KIO::WorkerFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kde.kio.worker.unpack" FILE "unpack.json")

public:
    std::unique_ptr<KIO::WorkerBase> createWorker(const QByteArray &pool, const QByteArray &app) override
    {
        return std::make_unique<UnpackWorker>(pool, app);
    }
};

// kioworker resolves this symbol and calls it with the protocol and the two
// sockets. The QCoreApplication is needed by KI18n, QStandardPaths and
// QMimeDatabase; exec() is never called, requests are served by KIO's blocking
// dispatch loop (docs/architecture.md, section 9).
extern "C" Q_DECL_EXPORT int kdemain(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("kio_unpack"));

    if (argc != 4) {
        std::fprintf(stderr, "Usage: kio_unpack protocol domain-socket1 domain-socket2\n");
        return 1;
    }

    UnpackWorker worker(argv[2], argv[3]);
    worker.dispatchLoop();
    return 0;
}

#include "main.moc"
