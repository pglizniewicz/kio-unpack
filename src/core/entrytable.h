/*
    SPDX-License-Identifier: GPL-2.0-or-later
    SPDX-FileCopyrightText: 2026 kio-unpack contributors
*/

#pragma once

#include <QByteArray>
#include <QHash>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace KioUnpack
{

enum class EntryType : std::uint8_t {
    Directory,
    File,
    Symlink,
    Other, // devices, FIFOs, sockets recorded by the format
};

/*
 * Contract 2 of docs/architecture.md, section 4: an opened volume is an
 * immutable table of entries, built once. Records have a fixed size and refer
 * to one string arena (section 4.2). The children of a directory are
 * contiguous and sorted by the bytes of their names. Names are UTF-8.
 */
class EntryTable
{
public:
    using Index = std::uint32_t;

    // Attributes of one entry as a backend reports them. Optional fields are
    // absent when the format does not record them.
    struct Attributes {
        EntryType type = EntryType::File;
        std::uint64_t size = 0;
        std::optional<std::int64_t> mtime; // seconds since the epoch
        std::optional<std::uint32_t> mode; // permission bits only
        std::optional<std::uint32_t> uid;
        std::optional<std::uint32_t> gid;
        QByteArray linkTarget; // symbolic links only, stored verbatim
        std::uint64_t handle = 0; // opaque to everyone but the backend
    };

    class Builder;

    EntryTable() = default;

    static constexpr Index root()
    {
        return 0;
    }
    Index count() const
    {
        return static_cast<Index>(m_records.size());
    }

    std::string_view name(Index index) const;
    EntryType type(Index index) const;
    std::uint64_t size(Index index) const;
    std::optional<std::int64_t> mtime(Index index) const;
    // The recorded permission bits, or synthesised ones for formats without
    // permissions: 0555 for directories, 0444 for everything else.
    std::uint32_t permissions(Index index) const;
    bool hasRecordedPermissions(Index index) const;
    std::optional<std::uint32_t> uid(Index index) const;
    std::optional<std::uint32_t> gid(Index index) const;
    std::string_view linkTarget(Index index) const;
    std::uint64_t handle(Index index) const;
    Index parent(Index index) const;

    // Children of a directory: indices [first, first + count).
    Index firstChild(Index directory) const;
    Index childCount(Index directory) const;

    // Exact lookup of one name among the children of a directory.
    std::optional<Index> child(Index directory, std::string_view name) const;

private:
    enum Flag : std::uint8_t {
        HasMtime = 1 << 0,
        HasMode = 1 << 1,
        HasUid = 1 << 2,
        HasGid = 1 << 3,
    };

    struct Record {
        std::uint64_t size;
        std::int64_t mtime;
        std::uint64_t handle;
        std::uint32_t nameOffset;
        std::uint32_t nameLength;
        std::uint32_t linkOffset;
        std::uint32_t linkLength;
        Index parent;
        Index firstChild;
        Index childCount;
        std::uint32_t mode;
        std::uint32_t uid;
        std::uint32_t gid;
        EntryType type;
        std::uint8_t flags;
    };

    const Record &record(Index index) const;
    std::string_view arenaView(std::uint32_t offset, std::uint32_t length) const;

    std::vector<Record> m_records;
    std::string m_arena;
};

/*
 * Collects entries in any order, by their path inside the volume, and builds
 * the table. Missing parent directories are created. Empty, "." and ".."
 * components are dropped, so no entry lands outside its directory; the path
 * "." (or an empty path) describes the root. A later entry with the same path
 * replaces the attributes of an earlier one. Children recorded below a node
 * that is not a directory are discarded.
 */
class EntryTable::Builder
{
public:
    Builder();

    void add(std::string_view path, const Attributes &attributes);
    EntryTable finish();

private:
    struct Node {
        QByteArray name;
        Attributes attributes;
        std::vector<std::uint32_t> children;
        QHash<QByteArray, std::uint32_t> childByName;
    };

    std::vector<Node> m_nodes;
};

} // namespace KioUnpack
