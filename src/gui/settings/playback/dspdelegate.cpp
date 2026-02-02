/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "dspdelegate.h"

#include "dspmodel.h"

namespace Fooyin {
DspDelegate::DspDelegate(QAbstractItemView* view, QObject* parent)
    : ActionDelegate{view, parent}
{
    QObject::connect(this, &ActionDelegate::buttonClicked, this, &DspDelegate::buttonClicked);
}

std::vector<ActionButton> DspDelegate::buttons(const QModelIndex& index) const
{
    std::vector<ActionButton> btns;

    btns.push_back({Button::Remove, QStringLiteral("X")});

    if(index.data(DspModel::HasSettings).toBool()) {
        btns.push_back({Button::Configure, QStringLiteral("…")});
    }

    return btns;
}

void DspDelegate::buttonClicked(const QModelIndex& index, int buttonId)
{
    switch(buttonId) {
        case(Button::Remove):
            emit removeClicked(index);
            break;
        case(Button::Configure):
            emit configureClicked(index);
            break;
    }
}
} // namespace Fooyin
