/*
    SPDX-License-Identifier: GPL-2.0-or-later
    SPDX-FileCopyrightText: 2026 kio-unpack contributors
*/

#include "core/pathresolver.h"
#include "core/error.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace KioUnpack;

namespace
{

// An in-memory volume, so resolver tests need no real image.
class FakeVolume : public Volume
{
public:
    explicit FakeVolume(EntryTable table)
        : m_table(std::move(table))
    {
    }
    const EntryTable &entries() const override
    {
        return m_table;
    }
    ExtractResult extract(EntryTable::Index, const DataSink &, const StopPredicate &) override
    {
        return ExtractResult::Completed;
    }

private:
    EntryTable m_table;
};

EntryTable sampleTable()
{
    EntryTable::Builder builder;
    EntryTable::Attributes file;
    file.type = EntryType::File;
    builder.add("a/b/x.iso", file);
    builder.add("dir/file.txt", file);
    EntryTable::Attributes link;
    link.type = EntryType::Symlink;
    link.linkTarget = QByteArrayLiteral("dir");
    builder.add("link", link);
    return builder.finish();
}

}

class PathResolverTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString m_image;
    QStringList m_opened;

    PathResolver resolver()
    {
        return PathResolver([this](const QString &imagePath, const StopPredicate &) -> std::unique_ptr<Volume> {
            m_opened.append(imagePath);
            return std::make_unique<FakeVolume>(sampleTable());
        });
    }

    ErrorKind kindOfResolving(const QString &path)
    {
        try {
            resolver().resolve(path);
        } catch (const Error &error) {
            return error.kind();
        }
        qFatal("resolving did not fail");
    }

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        // On disk: <tmp>/a/b/x.iso is a regular file inside nested directories,
        // and the image contains the same names again.
        QVERIFY(QDir(m_dir.path()).mkpath(QStringLiteral("a/b")));
        m_image = m_dir.filePath(QStringLiteral("a/b/x.iso"));
        QFile file(m_image);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("not read by the fake opener");
    }

    void init()
    {
        m_opened.clear();
    }

    // REQ-009
    void selectsShortestNonDirectoryPrefix()
    {
        const ResolvedPath resolved = resolver().resolve(m_image + QStringLiteral("/a/b/x.iso"));
        QCOMPARE(resolved.imagePath, m_image);
        QCOMPARE(resolved.innerPath, QStringLiteral("a/b/x.iso"));
        QCOMPARE(m_opened, QStringList{m_image});
        QCOMPARE(resolved.volume->entries().type(resolved.entry), EntryType::File);
    }

    // REQ-008
    void imageRootAndInnerDirectory()
    {
        const ResolvedPath root = resolver().resolve(m_image);
        QVERIFY(root.isImageRoot());
        QCOMPARE(root.entry, EntryTable::root());

        const ResolvedPath dir = resolver().resolve(m_image + QStringLiteral("/dir"));
        QCOMPARE(dir.innerPath, QStringLiteral("dir"));
        QCOMPARE(dir.volume->entries().type(dir.entry), EntryType::Directory);
    }

    // REQ-011
    void trailingAndRepeatedSlashesAreIgnored()
    {
        const QString doubled = m_image;
        const ResolvedPath plain = resolver().resolve(m_image + QStringLiteral("/dir/file.txt"));
        const ResolvedPath messy =
            resolver().resolve(QString(doubled).replace(QStringLiteral("/a/"), QStringLiteral("//a//")) + QStringLiteral("//dir///file.txt/"));
        QCOMPARE(messy.imagePath, plain.imagePath);
        QCOMPARE(messy.innerPath, plain.innerPath);
        QCOMPARE(messy.entry, plain.entry);

        QCOMPARE(PathResolver::normalise(QStringLiteral("//x//y/")), QStringLiteral("/x/y"));
        QCOMPARE(PathResolver::normalise(QStringLiteral("/")), QStringLiteral("/"));
    }

    // REQ-026: a symlink inside the image is never followed.
    void symlinkIsNotFollowed()
    {
        const ResolvedPath link = resolver().resolve(m_image + QStringLiteral("/link"));
        QCOMPARE(link.volume->entries().type(link.entry), EntryType::Symlink);
        QCOMPARE(kindOfResolving(m_image + QStringLiteral("/link/file.txt")), ErrorKind::NotFound);
    }

    void pathBelowFileIsFile()
    {
        QCOMPARE(kindOfResolving(m_image + QStringLiteral("/dir/file.txt/more")), ErrorKind::IsFile);
    }

    void missingInnerPathIsNotFound()
    {
        QCOMPARE(kindOfResolving(m_image + QStringLiteral("/nope")), ErrorKind::NotFound);
    }

    void missingDiskPathIsNotFound()
    {
        QCOMPARE(kindOfResolving(m_dir.filePath(QStringLiteral("missing/x.iso"))), ErrorKind::NotFound);
        QVERIFY(m_opened.isEmpty());
    }

    void directoryOnDiskIsNotAnImage()
    {
        QCOMPARE(kindOfResolving(m_dir.filePath(QStringLiteral("a/b"))), ErrorKind::Unsupported);
        QVERIFY(m_opened.isEmpty());
    }
};

QTEST_GUILESS_MAIN(PathResolverTest)

#include "pathresolvertest.moc"
