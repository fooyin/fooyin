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

#include "librarytreeitem.h"

#include <core/scripting/scriptparser.h>

#include <utils/treemodel.h>

namespace Fy {
namespace Gui::Widgets {
enum class LibraryTreeRole
{
    Title  = Qt::UserRole + 1,
    Tracks = Qt::UserRole + 2,
};

class LibraryTreeModel : public Utils::TreeModel<LibraryTreeItem>
{
    Q_OBJECT

public:
    explicit LibraryTreeModel(QObject* parent = nullptr);

    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;

    void setGroupScript(const QString& script);
    void beginReset();
    void reload(const Core::TrackList& tracks);
    void setupModelData(const Core::TrackList& tracks);

private:
    LibraryTreeItem* createNode(const QString& key, LibraryTreeItem* parent, const QString& title);

    Core::Scripting::Parser m_parser;
    QString m_groupScipt;
    std::unique_ptr<LibraryTreeItem> m_rootNode;
    std::unordered_map<QString, std::unique_ptr<LibraryTreeItem>> m_nodes;
};
} // namespace Gui::Widgets
} // namespace Fy
