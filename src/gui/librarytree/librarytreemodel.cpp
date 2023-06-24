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

#include <ranges>

namespace Fy::Gui::Widgets {
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

void LibraryTreeModel::setGroupScript(const QString& script)
{
    m_groupScipt = script;
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

    m_rootNode = std::make_unique<LibraryTreeItem>("", rootItem());
    rootItem()->appendChild(m_rootNode.get());

    const auto parsedField = m_parser.parse(m_groupScipt);

    auto filteredTracks = tracks | std::views::filter([](const Core::Track& track) {
                              return track.enabled();
                          });

    for(const Core::Track& track : filteredTracks) {
        const QString field = m_parser.evaluate(parsedField, track);
        if(field.isNull()) {
            continue;
        }

        LibraryTreeItem* parent = rootItem();

        const QStringList values = field.split(Core::Constants::Separator, Qt::SkipEmptyParts);
        for(const QString& value : values) {
            if(value.isNull()) {
                continue;
            }
            const QStringList levels = value.split("||");

            for(const QString& level : levels) {
                const QString title = level.trimmed();
                const QString key   = QString{"%1%2"}.arg(parent->title(), title);
                createNode(key, parent, title)->addTrack(track);
                parent = m_nodes.at(key).get();
            }
        }
        m_rootNode->addTrack(track);
    }
    m_rootNode->setTitle(QString{"All Music (%1)"}.arg(m_rootNode->trackCount()));
}

LibraryTreeItem* LibraryTreeModel::createNode(const QString& key, LibraryTreeItem* parent, const QString& title)
{
    if(!m_nodes.contains(key)) {
        m_nodes.emplace(key, std::make_unique<LibraryTreeItem>(title, parent));
    }
    LibraryTreeItem* treeItem = m_nodes.at(key).get();
    if(treeItem->pending()) {
        treeItem->setPending(false);
        parent->appendChild(treeItem);
    }
    return treeItem;
}
} // namespace Fy::Gui::Widgets
