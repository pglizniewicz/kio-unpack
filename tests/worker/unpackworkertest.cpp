/*
    SPDX-License-Identifier: GPL-2.0-or-later
    SPDX-FileCopyrightText: 2026 kio-unpack contributors
*/

// The built worker driven through KIO jobs, with QT_PLUGIN_PATH at the build
// tree (set by ecm_add_test).

#include <KIO/DeleteJob>
#include <KIO/ListJob>
#include <KIO/MkdirJob>
#include <KIO/SimpleJob>
#include <KIO/StatJob>
#include <KIO/StoredTransferJob>
#include <KIO/TransferJob>

#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTest>

#include <sys/stat.h>

namespace
{

const QString kFixtures = QStringLiteral(KIO_UNPACK_FIXTURES_DIR);

QUrl unpackUrl(const QString &image, const QString &inner = QString())
{
    QUrl url;
    url.setScheme(QStringLiteral("unpack"));
    url.setPath(kFixtures + u'/' + image + inner);
    return url;
}

QByteArray readSource(const QString &relative)
{
    QFile file(kFixtures + QStringLiteral("/rr-src/") + relative);
    if (!file.open(QIODevice::ReadOnly)) {
        qFatal("cannot read fixture source");
    }
    return file.readAll();
}

QByteArray checksum(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qFatal("cannot read fixture");
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&file);
    return hash.result();
}

}

class UnpackWorkerTest : public QObject
{
    Q_OBJECT

private:
    QByteArray m_rrChecksum;

    static KIO::UDSEntryList list(const QUrl &url)
    {
        KIO::ListJob *job = KIO::listDir(url, KIO::HideProgressInfo);
        KIO::UDSEntryList entries;
        connect(job, &KIO::ListJob::entries, job, [&entries](KIO::Job *, const KIO::UDSEntryList &batch) {
            entries += batch;
        });
        if (!job->exec()) {
            qWarning() << "listDir failed:" << job->errorString();
            return {};
        }
        return entries;
    }

    static KIO::UDSEntry statUrl(const QUrl &url, KIO::StatDetails details = KIO::StatDefaultDetails)
    {
        KIO::StatJob *job = KIO::stat(url, KIO::StatJob::SourceSide, details, KIO::HideProgressInfo);
        if (!job->exec()) {
            qWarning() << "stat failed:" << job->errorString();
            return {};
        }
        return job->statResult();
    }

    static QStringList names(const KIO::UDSEntryList &entries)
    {
        QStringList result;
        for (const KIO::UDSEntry &entry : entries) {
            result.append(entry.stringValue(KIO::UDSEntry::UDS_NAME));
        }
        result.sort();
        return result;
    }

    static KIO::UDSEntry find(const KIO::UDSEntryList &entries, const QString &name)
    {
        for (const KIO::UDSEntry &entry : entries) {
            if (entry.stringValue(KIO::UDSEntry::UDS_NAME) == name) {
                return entry;
            }
        }
        return {};
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        // REQ-035: the fixture is unchanged at the end (cleanupTestCase).
        m_rrChecksum = checksum(kFixtures + QStringLiteral("/rr.iso"));
    }

    void cleanupTestCase()
    {
        QCOMPARE(checksum(kFixtures + QStringLiteral("/rr.iso")), m_rrChecksum);
    }

    // REQ-008, REQ-021
    void listsImageRoot()
    {
        const KIO::UDSEntryList entries = list(unpackUrl(QStringLiteral("rr.iso")));
        QCOMPARE(names(entries),
                 (QStringList{QStringLiteral("."),
                              QStringLiteral("Dir_With_Long_Name"),
                              QStringLiteral("big.bin"),
                              QStringLiteral("dir"),
                              QStringLiteral("link.txt"),
                              QStringLiteral("noext"),
                              QStringLiteral("secret.txt")}));

        for (const KIO::UDSEntry &entry : entries) {
            QVERIFY(entry.contains(KIO::UDSEntry::UDS_NAME));
            QVERIFY(entry.contains(KIO::UDSEntry::UDS_FILE_TYPE));
            QVERIFY(entry.contains(KIO::UDSEntry::UDS_ACCESS));
            QVERIFY(entry.contains(KIO::UDSEntry::UDS_MODIFICATION_TIME));
            if (entry.isDir() || entry.isLink()) {
                continue;
            }
            QVERIFY(entry.contains(KIO::UDSEntry::UDS_SIZE));
        }
        QCOMPARE(find(entries, QStringLiteral("big.bin")).numberValue(KIO::UDSEntry::UDS_SIZE), readSource(QStringLiteral("big.bin")).size());
    }

    // REQ-022
    void listingHasExactlyOneDotEntry()
    {
        for (const QString &inner : {QString(), QStringLiteral("/dir")}) {
            const QStringList listed = names(list(unpackUrl(QStringLiteral("rr.iso"), inner)));
            QCOMPARE(listed.count(QStringLiteral(".")), 1);
            QCOMPARE(listed.count(QStringLiteral("..")), 0);
        }
    }

    // REQ-008
    void listsInnerDirectory()
    {
        QCOMPARE(names(list(unpackUrl(QStringLiteral("rr.iso"), QStringLiteral("/dir")))), (QStringList{QStringLiteral("."), QStringLiteral("file.txt")}));
    }

    // REQ-011
    void trailingSlashIsTheSameLocation()
    {
        QCOMPARE(names(list(unpackUrl(QStringLiteral("rr.iso"), QStringLiteral("/")))), names(list(unpackUrl(QStringLiteral("rr.iso")))));
        QCOMPARE(names(list(unpackUrl(QStringLiteral("rr.iso"), QStringLiteral("/dir/")))),
                 names(list(unpackUrl(QStringLiteral("rr.iso"), QStringLiteral("/dir")))));
    }

    // REQ-023
    void imageRootIsADirectoryWithTheImageType()
    {
        const QFileInfo image(kFixtures + QStringLiteral("/rr.iso"));
        const KIO::UDSEntry root = statUrl(unpackUrl(QStringLiteral("rr.iso")));
        QVERIFY(root.isDir());
        QCOMPARE(root.stringValue(KIO::UDSEntry::UDS_NAME), QStringLiteral("rr.iso"));
        QCOMPARE(root.stringValue(KIO::UDSEntry::UDS_MIME_TYPE), QStringLiteral("application/vnd.efi.iso"));
        QCOMPARE(root.numberValue(KIO::UDSEntry::UDS_MODIFICATION_TIME), image.lastModified().toSecsSinceEpoch());
        QCOMPARE(root.numberValue(KIO::UDSEntry::UDS_SIZE), image.size());

        const KIO::UDSEntry dot = find(list(unpackUrl(QStringLiteral("rr.iso"))), QStringLiteral("."));
        QVERIFY(dot.isDir());
        QCOMPARE(dot.stringValue(KIO::UDSEntry::UDS_MIME_TYPE), QStringLiteral("application/vnd.efi.iso"));
        QCOMPARE(dot.numberValue(KIO::UDSEntry::UDS_MODIFICATION_TIME), image.lastModified().toSecsSinceEpoch());
    }

    // REQ-024
    void rockRidgeNamesPermissionsAndOwners()
    {
        const KIO::UDSEntry secret = statUrl(unpackUrl(QStringLiteral("rr.iso"), QStringLiteral("/secret.txt")));
        QCOMPARE(secret.numberValue(KIO::UDSEntry::UDS_FILE_TYPE), S_IFREG);
        QCOMPARE(secret.numberValue(KIO::UDSEntry::UDS_ACCESS), 0700);
        QCOMPARE(secret.stringValue(KIO::UDSEntry::UDS_USER), QStringLiteral("1234"));
        QCOMPARE(secret.stringValue(KIO::UDSEntry::UDS_GROUP), QStringLiteral("5678"));

        const KIO::UDSEntryList longDir = list(unpackUrl(QStringLiteral("rr.iso"), QStringLiteral("/Dir_With_Long_Name")));
        QVERIFY(names(longDir).contains(QStringLiteral("A_Long_File_Name_With_MixedCase.txt")));

        // Directories are reported read-only (docs/architecture.md, section 10).
        const KIO::UDSEntry dir = statUrl(unpackUrl(QStringLiteral("rr.iso"), QStringLiteral("/dir")));
        QVERIFY(dir.isDir());
        QCOMPARE(dir.numberValue(KIO::UDSEntry::UDS_ACCESS), 0555);
        QCOMPARE(dir.stringValue(KIO::UDSEntry::UDS_MIME_TYPE), QStringLiteral("inode/directory"));
    }

    // REQ-025, through the worker
    void jolietOnlyImage()
    {
        const KIO::UDSEntryList entries = list(unpackUrl(QStringLiteral("joliet.iso"), QStringLiteral("/Dir_With_Long_Name")));
        QVERIFY(names(entries).contains(QStringLiteral("A_Long_File_Name_With_MixedCase.txt")));
        const KIO::UDSEntry file = find(entries, QStringLiteral("A_Long_File_Name_With_MixedCase.txt"));
        QCOMPARE(file.numberValue(KIO::UDSEntry::UDS_ACCESS), 0444);
        QVERIFY(!file.contains(KIO::UDSEntry::UDS_USER));
    }

    // REQ-026
    void symlinkIsReportedNotFollowed()
    {
        const KIO::UDSEntry listed = find(list(unpackUrl(QStringLiteral("rr.iso"))), QStringLiteral("link.txt"));
        QVERIFY(listed.isLink());
        QCOMPARE(listed.stringValue(KIO::UDSEntry::UDS_LINK_DEST), QStringLiteral("dir/file.txt"));

        const KIO::UDSEntry stated = statUrl(unpackUrl(QStringLiteral("rr.iso"), QStringLiteral("/link.txt")));
        QCOMPARE(stated.numberValue(KIO::UDSEntry::UDS_FILE_TYPE), S_IFLNK);
        QCOMPARE(stated.stringValue(KIO::UDSEntry::UDS_LINK_DEST), QStringLiteral("dir/file.txt"));
        QVERIFY(!stated.contains(KIO::UDSEntry::UDS_SIZE) || stated.numberValue(KIO::UDSEntry::UDS_SIZE) == 0);
    }

    // REQ-029
    void statReportsMimeTypeFromContent()
    {
        const KIO::UDSEntry entry = statUrl(unpackUrl(QStringLiteral("rr.iso"), QStringLiteral("/noext")), KIO::StatBasic | KIO::StatMimeType);
        QCOMPARE(entry.stringValue(KIO::UDSEntry::UDS_MIME_TYPE), QStringLiteral("text/plain"));
    }

    // REQ-008, REQ-030
    void getDeliversExactBytes()
    {
        KIO::StoredTransferJob *small =
            KIO::storedGet(unpackUrl(QStringLiteral("rr.iso"), QStringLiteral("/dir/file.txt")), KIO::NoReload, KIO::HideProgressInfo);
        QVERIFY2(small->exec(), qPrintable(small->errorString()));
        QCOMPARE(small->data(), readSource(QStringLiteral("dir/file.txt")));

        const QByteArray expected = readSource(QStringLiteral("big.bin"));
        KIO::TransferJob *job = KIO::get(unpackUrl(QStringLiteral("rr.iso"), QStringLiteral("/big.bin")), KIO::NoReload, KIO::HideProgressInfo);
        QByteArray received;
        bool sizeKnownBeforeData = true;
        int mimeTypeReports = 0;
        connect(job, &KIO::TransferJob::data, job, [&](KIO::Job *, const QByteArray &bytes) {
            if (received.isEmpty() && !bytes.isEmpty() && job->totalAmount(KJob::Bytes) != static_cast<qulonglong>(expected.size())) {
                sizeKnownBeforeData = false;
            }
            received += bytes;
        });
        connect(job, &KIO::TransferJob::mimeTypeFound, job, [&](KIO::Job *, const QString &) {
            ++mimeTypeReports;
        });
        QVERIFY2(job->exec(), qPrintable(job->errorString()));
        QCOMPARE(received.size(), expected.size());
        QVERIFY(received == expected);
        QVERIFY(sizeKnownBeforeData);
        QCOMPARE(mimeTypeReports, 1);
    }

    void getOnDirectoryFails()
    {
        KIO::StoredTransferJob *job = KIO::storedGet(unpackUrl(QStringLiteral("rr.iso"), QStringLiteral("/dir")), KIO::NoReload, KIO::HideProgressInfo);
        QVERIFY(!job->exec());
        QCOMPARE(job->error(), int(KIO::ERR_IS_DIRECTORY));
    }

    // REQ-035
    void writeOperationsAreUnsupported()
    {
        KIO::SimpleJob *put = KIO::put(unpackUrl(QStringLiteral("rr.iso"), QStringLiteral("/new.txt")), -1, KIO::HideProgressInfo);
        QVERIFY(!put->exec());
        QCOMPARE(put->error(), int(KIO::ERR_UNSUPPORTED_ACTION));

        KIO::SimpleJob *mkdir = KIO::mkdir(unpackUrl(QStringLiteral("rr.iso"), QStringLiteral("/newdir")));
        QVERIFY(!mkdir->exec());
        QCOMPARE(mkdir->error(), int(KIO::ERR_UNSUPPORTED_ACTION));

        KIO::SimpleJob *del = KIO::file_delete(unpackUrl(QStringLiteral("rr.iso"), QStringLiteral("/dir/file.txt")), KIO::HideProgressInfo);
        QVERIFY(!del->exec());
        QCOMPARE(del->error(), int(KIO::ERR_UNSUPPORTED_ACTION));
    }
};

QTEST_GUILESS_MAIN(UnpackWorkerTest)

#include "unpackworkertest.moc"
