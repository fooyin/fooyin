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

using namespace Qt::StringLiterals;

namespace Fooyin {
DspDelegate::DspDelegate(QAbstractItemView* view, QObject* parent, Mode mode)
    : ActionDelegate{view, parent}
    , m_mode{mode}
{
    QObject::connect(this, &ActionDelegate::buttonClicked, this, &DspDelegate::buttonWasClicked);
}

std::vector<ActionButton> DspDelegate::buttons(const QModelIndex& index) const
{
    std::vector<ActionButton> btns;

    if(m_mode == Mode::Available) {
        btns.push_back({.id = Add, .text = u"+"_s, .tooltip = tr("Add")});
        return btns;
    }

    btns.push_back({.id = Remove, .text = u"X"_s, .tooltip = tr("Remove")});

    if(index.data(DspModel::HasSettings).toBool()) {
        btns.push_back({.id = Configure, .text = u"…"_s, .tooltip = tr("Configure")});
    }

    return btns;
}

void DspDelegate::buttonWasClicked(const QModelIndex& index, int buttonId)
{
    switch(buttonId) {
        case Remove:
            Q_EMIT removeClicked(index);
            break;
        case Configure:
            Q_EMIT configureClicked(index);
            break;
        case Add:
            Q_EMIT addClicked(index);
            break;
    }
}
} // namespace Fooyin
