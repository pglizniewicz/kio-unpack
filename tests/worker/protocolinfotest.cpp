/*
    SPDX-License-Identifier: GPL-2.0-or-later
    SPDX-FileCopyrightText: 2026 kio-unpack contributors
*/

// Registration of the unpack protocol as KIO reads it from the plugin
// metadata embedded in build/bin/kf6/kio/kio_unpack.so.

#include <KProtocolInfo>
#include <KProtocolManager>

#include <QMimeDatabase>
#include <QStandardPaths>
#include <QTest>
#include <QUrl>

class ProtocolInfoTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    // REQ-001
    void registersTheProtocol()
    {
        QVERIFY(KProtocolInfo::isKnownProtocol(QStringLiteral("unpack")));
        QCOMPARE(KProtocolInfo::protocolClass(QStringLiteral("unpack")), QStringLiteral(":local"));
        QVERIFY(KProtocolManager::supportsReading(QUrl(QStringLiteral("unpack:/"))));
        QVERIFY(KProtocolManager::supportsListing(QUrl(QStringLiteral("unpack:/"))));
        QCOMPARE(KProtocolManager::inputType(QUrl(QStringLiteral("unpack:/"))), KProtocolInfo::T_FILESYSTEM);
        QCOMPARE(KProtocolManager::outputType(QUrl(QStringLiteral("unpack:/"))), KProtocolInfo::T_FILESYSTEM);
    }

    // REQ-035: the metadata does not offer writing.
    void declaresNoWriting()
    {
        const QUrl url(QStringLiteral("unpack:/"));
        QVERIFY(!KProtocolManager::supportsWriting(url));
        QVERIFY(!KProtocolManager::supportsDeleting(url));
        QVERIFY(!KProtocolManager::supportsMakeDir(url));
        QVERIFY(!KProtocolManager::supportsMoving(url));
    }

    // REQ-002
    void isoTypeComesFirst()
    {
        const QStringList types = KProtocolInfo::archiveMimetypes(QStringLiteral("unpack"));
        QVERIFY(!types.isEmpty());
        QCOMPARE(types.first(), QStringLiteral("application/vnd.efi.iso"));
        QCOMPARE(KProtocolManager::protocolForArchiveMimetype(QStringLiteral("application/vnd.efi.iso")), QStringLiteral("unpack"));
    }

    // REQ-003
    void onlyCanonicalNames()
    {
        const QMimeDatabase db;
        const QStringList types = KProtocolInfo::archiveMimetypes(QStringLiteral("unpack"));
        for (const QString &type : types) {
            QCOMPARE(db.mimeTypeForName(type).name(), type);
        }
    }

    // REQ-004
    void noTypeOfKioExtras()
    {
        const QStringList kioExtrasTypes{
            QStringLiteral("application/x-archive"),
            QStringLiteral("application/x-7z-compressed"),
            QStringLiteral("application/zip"),
            QStringLiteral("application/x-tar"),
            QStringLiteral("application/x-compressed-tar"),
            QStringLiteral("application/x-bzip-compressed-tar"),
            QStringLiteral("application/x-webarchive"),
            QStringLiteral("application/x-lzma-compressed-tar"),
            QStringLiteral("application/x-xz-compressed-tar"),
            QStringLiteral("application/x-zstd-compressed-tar"),
        };
        const QStringList types = KProtocolInfo::archiveMimetypes(QStringLiteral("unpack"));
        for (const QString &type : types) {
            QVERIFY2(!kioExtrasTypes.contains(type), qPrintable(type));
        }
    }
};

QTEST_GUILESS_MAIN(ProtocolInfoTest)

#include "protocolinfotest.moc"
