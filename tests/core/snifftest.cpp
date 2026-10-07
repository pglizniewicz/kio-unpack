/*
    SPDX-License-Identifier: GPL-2.0-or-later
    SPDX-FileCopyrightText: 2026 kio-unpack contributors
*/

#include "core/sniff.h"
#include "core/filebytesource.h"

#include <QFile>
#include <QRandomGenerator>
#include <QTemporaryDir>
#include <QTest>

using namespace KioUnpack;

class SniffTest : public QObject
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

private Q_SLOTS:
    void recognisesIso9660()
    {
        FileByteSource source(QStringLiteral(KIO_UNPACK_FIXTURES_DIR "/rr.iso"));
        QVERIFY(sniff(source).has(Signature::Iso9660));
    }

    void rejectsRandomBytes()
    {
        QByteArray noise(64 * 1024, Qt::Uninitialized);
        QRandomGenerator generator(42);
        for (char &c : noise) {
            c = static_cast<char>(generator.bounded(256));
        }
        FileByteSource source(writeFile(QStringLiteral("noise.iso"), noise));
        QVERIFY(sniff(source).signatures.isEmpty());
    }

    void rejectsShortFile()
    {
        FileByteSource source(writeFile(QStringLiteral("short.iso"), QByteArrayLiteral("CD001")));
        QVERIFY(sniff(source).signatures.isEmpty());
    }
};

QTEST_GUILESS_MAIN(SniffTest)

#include "snifftest.moc"
