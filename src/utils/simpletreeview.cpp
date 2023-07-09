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

#include <utils/simpletreeview.h>

namespace Fy::Utils {
int calculateMaxItemWidth(const SimpleTreeView* view, QAbstractItemModel* model, const QModelIndex& index)
{
    int maxItemWidth{0};

    const int itemWidth = view->sizeHintForIndex(index).width();
    maxItemWidth        = std::max(maxItemWidth, itemWidth);

    const int rowCount = model->rowCount(index);
    for(int row = 0; row < rowCount; ++row) {
        const QModelIndex childIndex = model->index(row, 0, index);
        const int childWidth         = calculateMaxItemWidth(view, model, childIndex);
        maxItemWidth                 = std::max(maxItemWidth, childWidth);
    }
    maxItemWidth += view->indentation();
    return maxItemWidth;
}

SimpleTreeView::SimpleTreeView(QWidget* parent)
    : QTreeView{parent}
{
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

QSize SimpleTreeView::sizeHint() const
{
    const int maxWidth = calculateMaxItemWidth(this, model(), {});
    return {maxWidth, 100};
}
} // namespace Fy::Utils
