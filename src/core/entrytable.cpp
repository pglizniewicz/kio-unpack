/*
    SPDX-License-Identifier: GPL-2.0-or-later
    SPDX-FileCopyrightText: 2026 kio-unpack contributors
*/

#include "entrytable.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace KioUnpack
{

namespace
{
constexpr std::uint32_t kSynthesisedDirectoryMode = 0555;
constexpr std::uint32_t kSynthesisedFileMode = 0444;
constexpr std::uint32_t kPermissionMask = 07777;

std::uint32_t checkedU32(std::size_t value)
{
    if (value > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("entry table exceeds 32-bit limits");
    }
    return static_cast<std::uint32_t>(value);
}
}

const EntryTable::Record &EntryTable::record(Index index) const
{
    return m_records.at(index);
}

std::string_view EntryTable::arenaView(std::uint32_t offset, std::uint32_t length) const
{
    return std::string_view(m_arena).substr(offset, length);
}

std::string_view EntryTable::name(Index index) const
{
    const Record &r = record(index);
    return arenaView(r.nameOffset, r.nameLength);
}

EntryType EntryTable::type(Index index) const
{
    return record(index).type;
}

std::uint64_t EntryTable::size(Index index) const
{
    return record(index).size;
}

std::optional<std::int64_t> EntryTable::mtime(Index index) const
{
    const Record &r = record(index);
    return (r.flags & HasMtime) ? std::optional<std::int64_t>(r.mtime) : std::nullopt;
}

std::uint32_t EntryTable::permissions(Index index) const
{
    const Record &r = record(index);
    if (r.flags & HasMode) {
        return r.mode & kPermissionMask;
    }
    return r.type == EntryType::Directory ? kSynthesisedDirectoryMode : kSynthesisedFileMode;
}

bool EntryTable::hasRecordedPermissions(Index index) const
{
    return record(index).flags & HasMode;
}

std::optional<std::uint32_t> EntryTable::uid(Index index) const
{
    const Record &r = record(index);
    return (r.flags & HasUid) ? std::optional<std::uint32_t>(r.uid) : std::nullopt;
}

std::optional<std::uint32_t> EntryTable::gid(Index index) const
{
    const Record &r = record(index);
    return (r.flags & HasGid) ? std::optional<std::uint32_t>(r.gid) : std::nullopt;
}

std::string_view EntryTable::linkTarget(Index index) const
{
    const Record &r = record(index);
    return arenaView(r.linkOffset, r.linkLength);
}

std::uint64_t EntryTable::handle(Index index) const
{
    return record(index).handle;
}

EntryTable::Index EntryTable::parent(Index index) const
{
    return record(index).parent;
}

EntryTable::Index EntryTable::firstChild(Index directory) const
{
    return record(directory).firstChild;
}

EntryTable::Index EntryTable::childCount(Index directory) const
{
    return record(directory).childCount;
}

std::optional<EntryTable::Index> EntryTable::child(Index directory, std::string_view childName) const
{
    const Record &dir = record(directory);
    Index low = dir.firstChild;
    Index high = dir.firstChild + dir.childCount;
    while (low < high) {
        const Index mid = low + (high - low) / 2;
        const int cmp = name(mid).compare(childName);
        if (cmp == 0) {
            return mid;
        }
        if (cmp < 0) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    return std::nullopt;
}

EntryTable::Builder::Builder()
{
    Node root;
    root.attributes.type = EntryType::Directory;
    m_nodes.push_back(std::move(root));
}

void EntryTable::Builder::add(std::string_view path, const Attributes &attributes)
{
    std::uint32_t current = 0;
    std::size_t position = 0;
    while (position <= path.size()) {
        const std::size_t slash = path.find('/', position);
        const std::size_t end = slash == std::string_view::npos ? path.size() : slash;
        const std::string_view component = path.substr(position, end - position);
        position = end + 1;

        if (component.empty() || component == "." || component == "..") {
            continue;
        }

        const QByteArray key(component.data(), static_cast<qsizetype>(component.size()));
        const auto found = m_nodes[current].childByName.constFind(key);
        if (found != m_nodes[current].childByName.constEnd()) {
            current = found.value();
            continue;
        }

        const auto index = checkedU32(m_nodes.size());
        Node node;
        node.name = key;
        node.attributes.type = EntryType::Directory; // until the entry itself arrives
        m_nodes.push_back(std::move(node));
        m_nodes[current].children.push_back(index);
        m_nodes[current].childByName.insert(key, index);
        current = index;
    }

    if (current == 0) {
        // The root stays a directory whatever the backend claims.
        Attributes rootAttributes = attributes;
        rootAttributes.type = EntryType::Directory;
        m_nodes[0].attributes = rootAttributes;
    } else {
        m_nodes[current].attributes = attributes;
    }
}

EntryTable EntryTable::Builder::finish()
{
    EntryTable table;
    table.m_records.reserve(m_nodes.size());

    auto appendString = [&table](const QByteArray &bytes) -> std::pair<std::uint32_t, std::uint32_t> {
        const auto offset = checkedU32(table.m_arena.size());
        table.m_arena.append(bytes.constData(), static_cast<std::size_t>(bytes.size()));
        checkedU32(table.m_arena.size());
        return {offset, checkedU32(static_cast<std::size_t>(bytes.size()))};
    };

    auto makeRecord = [&](const Node &node, Index parent) {
        Record r{};
        const auto [nameOffset, nameLength] = appendString(node.name);
        r.nameOffset = nameOffset;
        r.nameLength = nameLength;
        if (node.attributes.type == EntryType::Symlink) {
            const auto [linkOffset, linkLength] = appendString(node.attributes.linkTarget);
            r.linkOffset = linkOffset;
            r.linkLength = linkLength;
        }
        r.parent = parent;
        r.type = node.attributes.type;
        r.size = node.attributes.type == EntryType::Directory ? 0 : node.attributes.size;
        r.handle = node.attributes.handle;
        if (node.attributes.mtime) {
            r.mtime = *node.attributes.mtime;
            r.flags |= HasMtime;
        }
        if (node.attributes.mode) {
            r.mode = *node.attributes.mode;
            r.flags |= HasMode;
        }
        if (node.attributes.uid) {
            r.uid = *node.attributes.uid;
            r.flags |= HasUid;
        }
        if (node.attributes.gid) {
            r.gid = *node.attributes.gid;
            r.flags |= HasGid;
        }
        return r;
    };

    // Breadth-first: each directory's children are appended together, so they
    // are contiguous. `order[i]` is the builder node of record i.
    std::vector<std::uint32_t> order;
    order.reserve(m_nodes.size());
    order.push_back(0);
    table.m_records.push_back(makeRecord(m_nodes[0], 0));

    for (std::size_t i = 0; i < order.size(); ++i) {
        Node &node = m_nodes[order[i]];
        const auto recordIndex = static_cast<Index>(i);
        table.m_records[i].firstChild = checkedU32(table.m_records.size());
        table.m_records[i].childCount = 0;
        if (node.attributes.type != EntryType::Directory) {
            continue;
        }
        std::sort(node.children.begin(), node.children.end(), [this](std::uint32_t a, std::uint32_t b) {
            return std::string_view(m_nodes[a].name.constData(), static_cast<std::size_t>(m_nodes[a].name.size()))
                < std::string_view(m_nodes[b].name.constData(), static_cast<std::size_t>(m_nodes[b].name.size()));
        });
        for (const std::uint32_t childNode : node.children) {
            order.push_back(childNode);
            table.m_records.push_back(makeRecord(m_nodes[childNode], recordIndex));
        }
        table.m_records[i].childCount = checkedU32(node.children.size());
    }

    m_nodes.clear();
    return table;
}

} // namespace KioUnpack
