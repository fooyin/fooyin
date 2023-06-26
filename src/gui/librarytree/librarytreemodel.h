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

#pragma once

#include "librarytreegroup.h"

#include <core/models/trackfwd.h>
#include <core/scripting/scriptparser.h>

#include <utils/treeitem.h>
#include <utils/treemodel.h>

namespace Fy::Gui::Widgets {
enum LibraryTreeRole
{
    Title = Qt::UserRole,
    Tracks,
};

class LibraryTreeItem : public Utils::TreeItem<LibraryTreeItem>
{
public:
    enum Type
    {
        All,
        Normal
    };

    LibraryTreeItem();
    explicit LibraryTreeItem(QString title, LibraryTreeItem* parent, Type type = Normal);

    [[nodiscard]] bool pending() const;
    [[nodiscard]] QString title() const;
    [[nodiscard]] Core::TrackList tracks() const;
    [[nodiscard]] int trackCount() const;
    [[nodiscard]] QString key() const;

    void setPending(bool pending);
    void setTitle(const QString& title);
    void setKey(const QString& key);

    void addTrack(const Core::Track& track);

    void sortChildren();

private:
    bool m_pending;
    Type m_type;
    QString m_key;
    QString m_title;
    Core::TrackList m_tracks;
};

class LibraryTreeModel : public Utils::TreeModel<LibraryTreeItem>
{
    Q_OBJECT

public:
    explicit LibraryTreeModel(QObject* parent = nullptr);

    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;

    void changeGrouping(const LibraryTreeGrouping& grouping);
    void beginReset();
    void reload(const Core::TrackList& tracks);
    void setupModelData(const Core::TrackList& tracks);

private:
    LibraryTreeItem* createNode(const QString& key, LibraryTreeItem* parent, const QString& title);

    Core::Scripting::Registry m_registry;
    Core::Scripting::Parser m_parser;
    QString m_grouping;
    LibraryTreeItem m_allNode;
    std::unordered_map<QString, LibraryTreeItem> m_nodes;
};
} // namespace Fy::Gui::Widgets
