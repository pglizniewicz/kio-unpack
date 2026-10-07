/*
    SPDX-License-Identifier: GPL-2.0-or-later
    SPDX-FileCopyrightText: 2026 kio-unpack contributors
*/

#include "backends/libarchive/libarchivevolume.h"
#include "core/error.h"
#include "core/filebytesource.h"

#include <QFile>
#include <QTest>

using namespace KioUnpack;

namespace
{

const QString kFixtures = QStringLiteral(KIO_UNPACK_FIXTURES_DIR);

std::unique_ptr<Volume> openFixture(const QString &name, const StopPredicate &shouldStop = {})
{
    return LibarchiveVolume::open(std::make_unique<FileByteSource>(kFixtures + u'/' + name), shouldStop);
}

EntryTable::Index lookup(const EntryTable &table, std::initializer_list<std::string_view> path)
{
    EntryTable::Index current = EntryTable::root();
    for (const std::string_view component : path) {
        const auto next = table.child(current, component);
        if (!next) {
            qFatal("lookup failed");
        }
        current = *next;
    }
    return current;
}

QByteArray readSource(const QString &relative)
{
    QFile file(kFixtures + QStringLiteral("/rr-src/") + relative);
    if (!file.open(QIODevice::ReadOnly)) {
        qFatal("cannot read fixture source");
    }
    return file.readAll();
}

}

class LibarchiveVolumeTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    // REQ-024
    void rockRidgeNamesModesLinksAndOwners()
    {
        const auto volume = openFixture(QStringLiteral("rr.iso"));
        const EntryTable &table = volume->entries();

        const EntryTable::Index longName = lookup(table, {"Dir_With_Long_Name", "A_Long_File_Name_With_MixedCase.txt"});
        QCOMPARE(table.type(longName), EntryType::File);

        const EntryTable::Index secret = lookup(table, {"secret.txt"});
        QVERIFY(table.hasRecordedPermissions(secret));
        QCOMPARE(table.permissions(secret), 0700u);
        QCOMPARE(table.uid(secret), std::optional<std::uint32_t>(1234));
        QCOMPARE(table.gid(secret), std::optional<std::uint32_t>(5678));
        QVERIFY(table.mtime(secret).has_value());

        // REQ-026
        const EntryTable::Index link = lookup(table, {"link.txt"});
        QCOMPARE(table.type(link), EntryType::Symlink);
        QCOMPARE(table.linkTarget(link), std::string_view("dir/file.txt"));

        QCOMPARE(table.permissions(lookup(table, {"dir"})), 0755u);
        QCOMPARE(table.size(lookup(table, {"big.bin"})), static_cast<std::uint64_t>(readSource(QStringLiteral("big.bin")).size()));
    }

    // REQ-025
    void jolietNamesWithoutRockRidge()
    {
        const auto volume = openFixture(QStringLiteral("joliet.iso"));
        const EntryTable &table = volume->entries();

        const EntryTable::Index longName = lookup(table, {"Dir_With_Long_Name", "A_Long_File_Name_With_MixedCase.txt"});
        QCOMPARE(table.type(longName), EntryType::File);
        // The Joliet tree has no symlinks and no permissions or owners.
        QVERIFY(!table.child(EntryTable::root(), "link.txt"));
        const EntryTable::Index secret = lookup(table, {"secret.txt"});
        QVERIFY(!table.hasRecordedPermissions(secret));
        QCOMPARE(table.permissions(secret), 0444u);
        QCOMPARE(table.permissions(lookup(table, {"dir"})), 0555u);
        QVERIFY(!table.uid(secret));
    }

    // REQ-030 at the core level: exactly the file's bytes.
    void extractsExactBytes()
    {
        const auto volume = openFixture(QStringLiteral("rr.iso"));
        const EntryTable::Index big = lookup(volume->entries(), {"big.bin"});
        QByteArray out;
        const ExtractResult result = volume->extract(big,
                                                     [&out](const char *data, std::size_t length) {
                                                         out.append(data, static_cast<qsizetype>(length));
                                                         return true;
                                                     },
                                                     {});
        QCOMPARE(result, ExtractResult::Completed);
        QCOMPARE(out, readSource(QStringLiteral("big.bin")));

        QByteArray small;
        volume->extract(lookup(volume->entries(), {"dir", "file.txt"}),
                        [&small](const char *data, std::size_t length) {
                            small.append(data, static_cast<qsizetype>(length));
                            return true;
                        },
                        {});
        QCOMPARE(small, readSource(QStringLiteral("dir/file.txt")));
    }

    void sinkCanStopTheTransfer()
    {
        const auto volume = openFixture(QStringLiteral("rr.iso"));
        int calls = 0;
        const ExtractResult result = volume->extract(lookup(volume->entries(), {"big.bin"}),
                                                     [&calls](const char *, std::size_t) {
                                                         ++calls;
                                                         return false;
                                                     },
                                                     {});
        QCOMPARE(result, ExtractResult::Stopped);
        QCOMPARE(calls, 1);
    }

    void stopPredicateCancelsOpening()
    {
        try {
            openFixture(QStringLiteral("rr.iso"), [] {
                return true;
            });
            QFAIL("opening was not cancelled");
        } catch (const Error &error) {
            QCOMPARE(error.kind(), ErrorKind::Cancelled);
        }
    }

    void nonIsoDataIsUnsupported()
    {
        // The fixture's own source file is plain text, not an image.
        try {
            LibarchiveVolume::open(std::make_unique<FileByteSource>(kFixtures + QStringLiteral("/rr-src/dir/file.txt")), {});
            QFAIL("opening plain text succeeded");
        } catch (const Error &error) {
            QCOMPARE(error.kind(), ErrorKind::Unsupported);
        }
    }
};

QTEST_GUILESS_MAIN(LibarchiveVolumeTest)

#include "libarchivevolumetest.moc"
