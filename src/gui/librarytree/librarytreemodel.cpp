/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
 *
 * Fooyin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Fooyin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Fooyin.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "librarytreemodel.h"

#include <core/constants.h>

#include <QCryptographicHash>

#include <ranges>

namespace Fy::Gui::Widgets {
QString generateKey(const QString& parentKey, const QString& title)
{
    QCryptographicHash hash{QCryptographicHash::Md5};
    hash.addData(parentKey.toUtf8());
    hash.addData(title.toUtf8());

    QString headerKey = hash.result().toHex();
    return headerKey;
}

LibraryTreeItem::LibraryTreeItem()
    : LibraryTreeItem{"", nullptr}
{ }

LibraryTreeItem::LibraryTreeItem(QString title, LibraryTreeItem* parent, Type type)
    : TreeItem{parent}
    , m_pending{true}
    , m_type{type}
    , m_title{std::move(title)}
{ }

bool LibraryTreeItem::pending() const
{
    return m_pending;
}

QString LibraryTreeItem::title() const
{
    return m_title;
}

Core::TrackList LibraryTreeItem::tracks() const
{
    return m_tracks;
}

int LibraryTreeItem::trackCount() const
{
    return static_cast<int>(m_tracks.size());
}

QString LibraryTreeItem::key() const
{
    return m_key;
}

void LibraryTreeItem::setPending(bool pending)
{
    m_pending = pending;
}

void LibraryTreeItem::setTitle(const QString& title)
{
    m_title = title;
}

void LibraryTreeItem::setKey(const QString& key)
{
    m_key = key;
}

void LibraryTreeItem::addTrack(const Core::Track& track)
{
    m_tracks.emplace_back(track);
}

void LibraryTreeItem::sortChildren()
{
    std::vector<LibraryTreeItem*> sortedChildren{m_children};
    std::sort(sortedChildren.begin(), sortedChildren.end(), [](const LibraryTreeItem* lhs, const LibraryTreeItem* rhs) {
        if(lhs->m_type == All) {
            return true;
        }
        if(rhs->m_type == All) {
            return false;
        }
        const auto cmp = QString::localeAwareCompare(lhs->m_title, rhs->m_title);
        if(cmp == 0) {
            return false;
        }
        return cmp < 0;
    });
    m_children = sortedChildren;

    for(auto& child : m_children) {
        child->sortChildren();
    }
}

LibraryTreeModel::LibraryTreeModel(QObject* parent)
    : TreeModel{parent}
    , m_parser{&m_registry}
{ }

QVariant LibraryTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(section)

    if(orientation == Qt::Orientation::Vertical) {
        return {};
    }

    if(role == Qt::TextAlignmentRole) {
        return (Qt::AlignHCenter);
    }

    if(role != Qt::DisplayRole) {
        return {};
    }

    return "Library Tree";
}

QVariant LibraryTreeModel::data(const QModelIndex& index, int role) const
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    auto* item = static_cast<LibraryTreeItem*>(index.internalPointer());

    if(role == Qt::DisplayRole) {
        const QString& name = item->title();
        return !name.isEmpty() ? name : "?";
    }

    const auto type = static_cast<LibraryTreeRole>(role);
    if(type == LibraryTreeRole::Title) {
        return item->title();
    }

    if(type == LibraryTreeRole::Tracks) {
        return QVariant::fromValue(item->tracks());
    }
    return {};
}

void LibraryTreeModel::changeGrouping(const LibraryTreeGrouping& grouping)
{
    m_grouping = grouping.script;
}

void LibraryTreeModel::beginReset()
{
    m_nodes.clear();
    resetRoot();
}

void LibraryTreeModel::reload(const Core::TrackList& tracks)
{
    beginResetModel();
    beginReset();
    setupModelData(tracks);
    endResetModel();
}

void LibraryTreeModel::setupModelData(const Core::TrackList& tracks)
{
    if(tracks.empty()) {
        return;
    }

    m_allNode = LibraryTreeItem{"", rootItem(), LibraryTreeItem::All};

    rootItem()->appendChild(&m_allNode);

    const auto parsedField = m_parser.parse(m_grouping);

    auto filteredTracks = tracks | std::views::filter([](const Core::Track& track) {
                              return track.enabled();
                          });

    for(const Core::Track& track : filteredTracks) {
        const QString field = m_parser.evaluate(parsedField, track);
        if(field.isNull()) {
            continue;
        }

        const QStringList values = field.split(Core::Constants::Separator, Qt::SkipEmptyParts);
        for(const QString& value : values) {
            if(value.isNull()) {
                continue;
            }
            const QStringList levels = value.split("||");

            LibraryTreeItem* parent = rootItem();

            for(const QString& level : levels) {
                const QString title = level.trimmed();
                const QString key   = generateKey(parent->key(), title);
                auto* node          = createNode(key, parent, title);
                node->addTrack(track);
                parent = node;
            }
        }
        m_allNode.addTrack(track);
    }
    m_allNode.setTitle(QString{"All Music (%1)"}.arg(m_allNode.trackCount()));
    rootItem()->sortChildren();
}

LibraryTreeItem* LibraryTreeModel::createNode(const QString& key, LibraryTreeItem* parent, const QString& title)
{
    if(!m_nodes.contains(key)) {
        m_nodes.emplace(key, LibraryTreeItem{title, parent});
    }
    LibraryTreeItem* treeItem = &m_nodes.at(key);
    if(treeItem->pending()) {
        treeItem->setPending(false);
        treeItem->setKey(key);
        parent->appendChild(treeItem);
    }
    return treeItem;
}
} // namespace Fy::Gui::Widgets
