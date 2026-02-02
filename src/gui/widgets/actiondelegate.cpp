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

#include <gui/widgets/actiondelegate.h>

#include <QAbstractItemView>
#include <QApplication>
#include <QMouseEvent>
#include <QPainter>

constexpr auto ButtonSize    = 20;
constexpr auto ButtonSpacing = 4;
constexpr auto ButtonMargin  = 4;
constexpr auto ButtonRadius  = 3;

namespace {
QRect buttonRect(const QStyleOptionViewItem& option, int position)
{
    // Buttons are laid out right-to-left: position 0 is rightmost
    const int fromRight = position;
    const int x         = option.rect.right() - ButtonMargin - ButtonSize - (fromRight * (ButtonSize + ButtonSpacing));
    const int y         = option.rect.top() + ((option.rect.height() - ButtonSize) / 2);
    return {x, y, ButtonSize, ButtonSize};
}

int buttonsWidth(int count)
{
    if(count <= 0) {
        return 0;
    }

    return ButtonMargin + (count * ButtonSize) + ((count - 1) * ButtonSpacing);
}
} // namespace

namespace Fooyin {
ActionDelegate::ActionDelegate(QAbstractItemView* view, QObject* parent)
    : QStyledItemDelegate{parent}
    , m_view{view}
{ }

void ActionDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt{option};
    initStyleOption(&opt, index);

    painter->save();

    const QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);

    const auto btns      = buttons(index);
    const int btnCount   = static_cast<int>(btns.size());
    const int totalWidth = buttonsWidth(btnCount);

    QRect textRect{opt.rect};
    textRect.setRight(textRect.right() - totalWidth);

    const auto textRole = opt.state & QStyle::State_Selected ? QPalette::HighlightedText : QPalette::NoRole;
    const QString text  = index.data(Qt::DisplayRole).toString();
    style->drawItemText(painter, textRect, Qt::AlignVCenter | Qt::AlignLeft, opt.palette,
                        opt.state & QStyle::State_Enabled, text, textRole);

    const bool isHoveredRow = (m_hoveredIndex.isValid() && m_hoveredIndex == index);

    painter->setRenderHint(QPainter::Antialiasing);

    for(int i{0}; i < btnCount; ++i) {
        const QRect rect = buttonRect(opt, i);

        if(isHoveredRow && m_hoveredButton == i) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(opt.palette.color(QPalette::Button));
            painter->drawRoundedRect(rect, ButtonRadius, ButtonRadius);
        }

        painter->setPen(QPalette::ButtonText);
        painter->setBrush(Qt::NoBrush);
        painter->drawText(rect, Qt::AlignCenter, btns[i].text);
    }

    painter->restore();
}

QSize ActionDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    size.setHeight(std::max(size.height(), ButtonSize + (2 * ButtonMargin)));
    return size;
}

bool ActionDelegate::editorEvent(QEvent* event, QAbstractItemModel* /*model*/, const QStyleOptionViewItem& option,
                                 const QModelIndex& index)
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    const auto btns    = buttons(index);
    const int btnCount = static_cast<int>(btns.size());
    const auto type    = event->type();

    if(type == QEvent::MouseMove) {
        const auto* mouseEvent = static_cast<QMouseEvent*>(event);
        const QPoint pos       = mouseEvent->position().toPoint();

        int hoveredBtn{-1};
        for(int i{0}; i < btnCount; ++i) {
            if(buttonRect(opt, i).contains(pos)) {
                hoveredBtn = i;
                break;
            }
        }

        if(m_hoveredIndex != index || m_hoveredButton != hoveredBtn) {
            m_hoveredIndex  = index;
            m_hoveredButton = hoveredBtn;

            m_view->viewport()->update();
        }

        return false;
    }

    if(type != QEvent::MouseButtonRelease) {
        return false;
    }

    const auto* mouseEvent = static_cast<QMouseEvent*>(event);
    if(mouseEvent->button() != Qt::LeftButton) {
        return false;
    }

    const QPoint pos = mouseEvent->position().toPoint();

    for(int i{0}; i < btnCount; ++i) {
        if(buttonRect(opt, i).contains(pos)) {
            emit buttonClicked(index, btns[i].id);
            return true;
        }
    }

    return false;
}
} // namespace Fooyin
