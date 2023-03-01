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

#include "infoitem.h"

#include <utils/treemodel.h>

namespace Fy {

namespace Core {
class Track;

namespace Player {
class PlayerManager;
}
} // namespace Core

namespace Gui::Widgets {
namespace Info {
enum Role
{
    Type = Qt::UserRole + 20,
};
}

class InfoModel : public Utils::TreeModel<InfoItem>
{
public:
    explicit InfoModel(Core::Player::PlayerManager* playerManager, QObject* parent = nullptr);

    void setupModel();
    void addNode(InfoItem::Type type, const QString& title, InfoItem* parent);
    void reset();

    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;

private:
    Core::Player::PlayerManager* m_playerManager;
    std::vector<std::unique_ptr<InfoItem>> m_nodes;
};
} // namespace Gui::Widgets
} // namespace Fy
