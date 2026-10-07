/*
    SPDX-License-Identifier: GPL-2.0-or-later
    SPDX-FileCopyrightText: 2026 kio-unpack contributors
*/

#include "core/entrytable.h"

#include <QStringList>
#include <QTest>

using namespace KioUnpack;

namespace
{

EntryTable::Attributes file(std::uint64_t size, std::uint64_t handle = 0)
{
    EntryTable::Attributes a;
    a.type = EntryType::File;
    a.size = size;
    a.handle = handle;
    return a;
}

EntryTable::Attributes directory()
{
    EntryTable::Attributes a;
    a.type = EntryType::Directory;
    return a;
}

QStringList childNames(const EntryTable &table, EntryTable::Index dir)
{
    QStringList names;
    for (EntryTable::Index i = table.firstChild(dir); i < table.firstChild(dir) + table.childCount(dir); ++i) {
        names.append(QString::fromUtf8(table.name(i)));
    }
    return names;
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

}

class EntryTableTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void childrenAreSortedAndContiguous()
    {
        EntryTable::Builder builder;
        builder.add("b/z.txt", file(1));
        builder.add("a.txt", file(2));
        builder.add("b", directory());
        builder.add("B.txt", file(3));
        builder.add("b/y.txt", file(4));
        const EntryTable table = builder.finish();

        // Sorted by name bytes: upper case before lower case.
        QCOMPARE(childNames(table, EntryTable::root()), (QStringList{QStringLiteral("B.txt"), QStringLiteral("a.txt"), QStringLiteral("b")}));
        const EntryTable::Index b = lookup(table, {"b"});
        QCOMPARE(childNames(table, b), (QStringList{QStringLiteral("y.txt"), QStringLiteral("z.txt")}));
        QCOMPARE(table.parent(lookup(table, {"b", "y.txt"})), b);
        QCOMPARE(table.size(lookup(table, {"b", "z.txt"})), 1u);
    }

    void createsMissingParentDirectories()
    {
        EntryTable::Builder builder;
        builder.add("c/d/e.txt", file(5, 7));
        const EntryTable table = builder.finish();

        const EntryTable::Index d = lookup(table, {"c", "d"});
        QCOMPARE(table.type(d), EntryType::Directory);
        QCOMPARE(table.permissions(d), 0555u);
        QVERIFY(!table.hasRecordedPermissions(d));
        const EntryTable::Index e = lookup(table, {"c", "d", "e.txt"});
        QCOMPARE(table.handle(e), 7u);
        QCOMPARE(table.permissions(e), 0444u);
    }

    void exactLookupMisses()
    {
        EntryTable::Builder builder;
        builder.add("Readme.txt", file(1));
        const EntryTable table = builder.finish();

        QVERIFY(table.child(EntryTable::root(), "Readme.txt"));
        QVERIFY(!table.child(EntryTable::root(), "readme.txt"));
        QVERIFY(!table.child(EntryTable::root(), "Readme"));
        QVERIFY(!table.child(EntryTable::root(), ""));
    }

    void rootTakesTheDotEntry()
    {
        EntryTable::Builder builder;
        EntryTable::Attributes root = directory();
        root.mode = 0750;
        root.mtime = 1234567890;
        builder.add(".", root);
        builder.add("./x", file(1));
        const EntryTable table = builder.finish();

        QCOMPARE(table.permissions(EntryTable::root()), 0750u);
        QCOMPARE(table.mtime(EntryTable::root()), std::optional<std::int64_t>(1234567890));
        QCOMPARE(childNames(table, EntryTable::root()), QStringList{QStringLiteral("x")});
    }

    void dotComponentsCannotEscape()
    {
        EntryTable::Builder builder;
        builder.add("../../escape.txt", file(1));
        builder.add("dir/../inside.txt", file(2));
        builder.add("//double//slash", file(3));
        const EntryTable table = builder.finish();

        QCOMPARE(childNames(table, EntryTable::root()), (QStringList{QStringLiteral("dir"), QStringLiteral("double"), QStringLiteral("escape.txt")}));
        QCOMPARE(childNames(table, lookup(table, {"dir"})), QStringList{QStringLiteral("inside.txt")});
    }

    void laterEntryReplacesAttributes()
    {
        EntryTable::Builder builder;
        builder.add("f", file(1));
        builder.add("f", file(9));
        const EntryTable table = builder.finish();
        QCOMPARE(table.size(lookup(table, {"f"})), 9u);
    }

    void childrenBelowAFileAreDiscarded()
    {
        EntryTable::Builder builder;
        builder.add("f", file(1));
        builder.add("f/g", file(2));
        const EntryTable table = builder.finish();
        const EntryTable::Index f = lookup(table, {"f"});
        QCOMPARE(table.type(f), EntryType::File);
        QCOMPARE(table.childCount(f), 0u);
        QCOMPARE(table.count(), 2u);
    }

    void symlinkTargetAndOptionalFields()
    {
        EntryTable::Builder builder;
        EntryTable::Attributes link;
        link.type = EntryType::Symlink;
        link.linkTarget = QByteArrayLiteral("../target");
        link.uid = 1000;
        builder.add("link", link);
        builder.add("plain", file(1));
        const EntryTable table = builder.finish();

        const EntryTable::Index l = lookup(table, {"link"});
        QCOMPARE(table.linkTarget(l), std::string_view("../target"));
        QCOMPARE(table.uid(l), std::optional<std::uint32_t>(1000));
        QVERIFY(!table.gid(l));
        const EntryTable::Index p = lookup(table, {"plain"});
        QVERIFY(table.linkTarget(p).empty());
        QVERIFY(!table.mtime(p));
    }
};

QTEST_GUILESS_MAIN(EntryTableTest)

#include "entrytabletest.moc"
