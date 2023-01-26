/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include "quicksetupmodel.h"

#include "gui/layoutprovider.h"

namespace Gui {
QuickSetupModel::QuickSetupModel(LayoutProvider* layoutProvider, QObject* parent)
    : QAbstractListModel{parent}
    , m_layoutProvider{layoutProvider}
{ }

int QuickSetupModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return static_cast<int>(m_layoutProvider->layouts().size());
}

QVariant QuickSetupModel::data(const QModelIndex& index, int role) const
{
    Q_UNUSED(role)
    if(!index.isValid() && !checkIndex(index)) {
        return {};
    }

    switch(role) {
        case(Qt::DisplayRole): {
            return m_layoutProvider->layouts().at(index.row()).name;
        }
        case(QuickSetupRole::Layout): {
            return m_layoutProvider->layouts().at(index.row()).json;
        }
        default: {
            return {};
        }
    }

    return {};
}
} // namespace Gui
