/*
    SPDX-License-Identifier: GPL-2.0-or-later
    SPDX-FileCopyrightText: 2026 kio-unpack contributors
*/

// kio-unpack-cli: list, stat and cat over libkio-unpack-core, without KIO
// (REQ-103). A development tool for smoke tests, corpus runs and fuzz seeds.

#include "core/error.h"
#include "core/pathresolver.h"

#include <KLocalizedString>

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDateTime>
#include <QTimeZone>

#include <cstdio>
#include <iostream>

using namespace KioUnpack;

namespace
{

QString typeName(EntryType type)
{
    switch (type) {
    case EntryType::Directory:
        return QStringLiteral("dir");
    case EntryType::File:
        return QStringLiteral("file");
    case EntryType::Symlink:
        return QStringLiteral("link");
    case EntryType::Other:
        return QStringLiteral("other");
    }
    return QString();
}

QString longLine(const EntryTable &table, EntryTable::Index index)
{
    QString line = typeName(table.type(index)) + u' ' + QString::number(table.permissions(index), 8).rightJustified(4, u'0') + u' '
        + QString::number(table.size(index)) + u' ';
    if (const auto mtime = table.mtime(index)) {
        line += QDateTime::fromSecsSinceEpoch(*mtime, QTimeZone::UTC).toString(Qt::ISODate);
    } else {
        line += u'-';
    }
    line += u' ' + QString::fromUtf8(table.name(index));
    if (table.type(index) == EntryType::Symlink) {
        line += QStringLiteral(" -> ") + QString::fromUtf8(table.linkTarget(index));
    }
    return line;
}

void printLine(const QString &line)
{
    std::cout << line.toUtf8().constData() << '\n';
}

int list(const ResolvedPath &resolved, bool longFormat)
{
    const EntryTable &table = resolved.volume->entries();
    if (table.type(resolved.entry) != EntryType::Directory) {
        throw Error::isFile(resolved.imagePath + u'/' + resolved.innerPath);
    }
    const EntryTable::Index first = table.firstChild(resolved.entry);
    const EntryTable::Index count = table.childCount(resolved.entry);
    for (EntryTable::Index i = first; i < first + count; ++i) {
        printLine(longFormat ? longLine(table, i) : QString::fromUtf8(table.name(i)));
    }
    return 0;
}

int stat(const ResolvedPath &resolved)
{
    const EntryTable &table = resolved.volume->entries();
    const EntryTable::Index i = resolved.entry;
    printLine(QStringLiteral("NAME ") + (resolved.isImageRoot() ? resolved.imagePath.section(u'/', -1) : QString::fromUtf8(table.name(i))));
    printLine(QStringLiteral("TYPE ") + typeName(table.type(i)));
    printLine(QStringLiteral("ACCESS ") + QString::number(table.permissions(i), 8).rightJustified(4, u'0'));
    printLine(QStringLiteral("SIZE ") + QString::number(table.size(i)));
    if (const auto mtime = table.mtime(i)) {
        printLine(QStringLiteral("MODIFICATION_TIME ") + QDateTime::fromSecsSinceEpoch(*mtime, QTimeZone::UTC).toString(Qt::ISODate));
    }
    if (const auto uid = table.uid(i)) {
        printLine(QStringLiteral("USER ") + QString::number(*uid));
    }
    if (const auto gid = table.gid(i)) {
        printLine(QStringLiteral("GROUP ") + QString::number(*gid));
    }
    if (table.type(i) == EntryType::Symlink) {
        printLine(QStringLiteral("LINK_DEST ") + QString::fromUtf8(table.linkTarget(i)));
    }
    return 0;
}

int cat(const ResolvedPath &resolved)
{
    const EntryTable &table = resolved.volume->entries();
    const EntryType type = table.type(resolved.entry);
    if (type == EntryType::Directory) {
        throw Error::isDirectory(resolved.imagePath + u'/' + resolved.innerPath);
    }
    if (type == EntryType::Symlink) {
        throw Error::cannotOpen(resolved.imagePath + u'/' + resolved.innerPath, i18n("Symbolic links are not followed."));
    }
    bool writeFailed = false;
    resolved.volume->extract(resolved.entry,
                             [&writeFailed](const char *data, std::size_t length) {
                                 if (std::fwrite(data, 1, length, stdout) != length) {
                                     writeFailed = true;
                                     return false;
                                 }
                                 return true;
                             },
                             {});
    if (std::fflush(stdout) != 0 || writeFailed) {
        std::cerr << i18n("Writing to standard output failed.").toUtf8().constData() << '\n';
        return 1;
    }
    return 0;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("kio-unpack-cli"));

    QCommandLineParser parser;
    parser.setApplicationDescription(i18n("Lists and reads disc images and archives with the kio-unpack core, without KIO."));
    parser.addHelpOption();
    parser.addPositionalArgument(QStringLiteral("command"), i18n("One of list, stat or cat."));
    parser.addPositionalArgument(QStringLiteral("path"), i18n("Absolute path of an image, optionally followed by a path inside it, as in an unpack: URL."));
    const QCommandLineOption longOption(QStringList{QStringLiteral("l"), QStringLiteral("long")}, i18n("list: show type, permissions, size and time."));
    parser.addOption(longOption);
    parser.process(app);

    const QStringList arguments = parser.positionalArguments();
    if (arguments.size() != 2) {
        std::cerr << i18n("Expected a command and a path.").toUtf8().constData() << '\n';
        return 2;
    }
    const QString &command = arguments.at(0);
    QString path = arguments.at(1);
    if (path.startsWith(QLatin1String("unpack:"))) {
        path.remove(0, int(sizeof("unpack:") - 1));
    }

    try {
        const PathResolver resolver;
        if (command == QLatin1String("list")) {
            return list(resolver.resolve(path), parser.isSet(longOption));
        }
        if (command == QLatin1String("stat")) {
            return stat(resolver.resolve(path));
        }
        if (command == QLatin1String("cat")) {
            return cat(resolver.resolve(path));
        }
        std::cerr << i18n("Unknown command: %1", command).toUtf8().constData() << '\n';
        return 2;
    } catch (const Error &error) {
        std::cerr << error.message().toUtf8().constData() << '\n';
        return 1;
    }
}
