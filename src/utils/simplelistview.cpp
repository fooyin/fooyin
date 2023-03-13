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

#include <utils/simplelistview.h>

namespace Fy::Utils {
SimpleListViewDelegate::SimpleListViewDelegate(QObject* parent)
    : QStyledItemDelegate{parent}
    , m_height{0}
{ }

void SimpleListViewDelegate::setHeight(int height)
{
    m_height = height;
}

QSize SimpleListViewDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    size.setHeight(std::max(size.height(), (25 + m_height)));
    return size;
}

SimpleListView::SimpleListView(QWidget* parent)
    : QListView{parent}
    , m_delegate{new SimpleListViewDelegate(this)}
    , m_width{0}
{
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding);
    setItemDelegate(m_delegate);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

void SimpleListView::setWidth(int width)
{
    m_width = width;
    updateGeometry();
}

void SimpleListView::setHeight(int height)
{
    m_delegate->setHeight(height);
    updateGeometry();
}

QSize SimpleListView::sizeHint() const
{
    int width = sizeHintForColumn(0) + (frameWidth() * 2) + 5;
    width += verticalScrollBar()->sizeHint().width();
    width += m_width;
    return {width, 100};
}
} // namespace Fy::Utils
