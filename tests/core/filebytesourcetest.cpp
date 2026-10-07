/*
    SPDX-License-Identifier: GPL-2.0-or-later
    SPDX-FileCopyrightText: 2026 kio-unpack contributors
*/

#include "core/filebytesource.h"
#include "core/error.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <sys/stat.h>
#include <unistd.h>

using namespace KioUnpack;

class FileByteSourceTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    QString writeFile(const QString &name, const QByteArray &content)
    {
        const QString path = m_dir.filePath(name);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(content) != content.size()) {
            qFatal("cannot write test file");
        }
        return path;
    }

    static ErrorKind kindOfOpening(const QString &path)
    {
        try {
            FileByteSource source(path);
        } catch (const Error &error) {
            return error.kind();
        }
        qFatal("opening did not fail");
    }

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
    }

    void readsAtOffsets()
    {
        FileByteSource source(writeFile(QStringLiteral("digits"), QByteArrayLiteral("0123456789")));
        QCOMPARE(source.size(), 10u);

        char buffer[16] = {};
        QCOMPARE(source.readAt(3, buffer, 4), 4u);
        QCOMPARE(QByteArray(buffer, 4), QByteArrayLiteral("3456"));
    }

    void shortReadOnlyAtEnd()
    {
        FileByteSource source(writeFile(QStringLiteral("short"), QByteArrayLiteral("0123456789")));
        char buffer[16] = {};
        QCOMPARE(source.readAt(8, buffer, sizeof(buffer)), 2u);
        QCOMPARE(QByteArray(buffer, 2), QByteArrayLiteral("89"));
        QCOMPARE(source.readAt(10, buffer, sizeof(buffer)), 0u);
        QCOMPARE(source.readAt(1000, buffer, sizeof(buffer)), 0u);
    }

    void recordsIdentity()
    {
        const QString path = writeFile(QStringLiteral("identity"), QByteArrayLiteral("abc"));
        FileByteSource source(path);
        struct stat st{};
        QCOMPARE(::stat(QFile::encodeName(path).constData(), &st), 0);
        QCOMPARE(source.identity().inode, st.st_ino);
        QCOMPARE(source.identity().device, st.st_dev);
        QCOMPARE(source.identity().size, 3u);
    }

    void missingFileIsNotFound()
    {
        QCOMPARE(kindOfOpening(m_dir.filePath(QStringLiteral("missing"))), ErrorKind::NotFound);
    }

    void directoryCannotBeOpened()
    {
        QCOMPARE(kindOfOpening(m_dir.path()), ErrorKind::CannotOpen);
    }

    void fifoIsRejectedWithoutBlocking()
    {
        const QString path = m_dir.filePath(QStringLiteral("fifo"));
        QCOMPARE(::mkfifo(QFile::encodeName(path).constData(), 0600), 0);
        QCOMPARE(kindOfOpening(path), ErrorKind::CannotOpen);
    }

    void unreadableFileIsAccessDenied()
    {
        if (::geteuid() == 0) {
            QSKIP("root can read files without permissions");
        }
        const QString path = writeFile(QStringLiteral("unreadable"), QByteArrayLiteral("x"));
        QVERIFY(QFile::setPermissions(path, QFileDevice::Permissions()));
        QCOMPARE(kindOfOpening(path), ErrorKind::AccessDenied);
    }
};

QTEST_GUILESS_MAIN(FileByteSourceTest)

#include "filebytesourcetest.moc"
