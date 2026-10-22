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
#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>

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

QRect checkRect(const QStyleOptionViewItem& option, const QStyle* style)
{
    QStyleOptionViewItem checkOpt{option};
    checkOpt.features |= QStyleOptionViewItem::HasCheckIndicator;
    return style->subElementRect(QStyle::SE_ItemViewItemCheckIndicator, &checkOpt, option.widget);
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

    const QStyle* style  = opt.widget ? opt.widget->style() : QApplication::style();
    const auto btns      = buttons(index);
    const int btnCount   = static_cast<int>(btns.size());
    const int totalWidth = buttonsWidth(btnCount);

    if(totalWidth > 0) {
        const QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, opt.widget);
        const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, &opt, opt.widget) + 1;
        const int textWidth  = std::max(0, textRect.width() - totalWidth - (2 * textMargin));
        opt.text             = opt.fontMetrics.elidedText(opt.text, opt.textElideMode, textWidth);
    }

    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    const bool isHoveredRow = (m_hoveredIndex.isValid() && m_hoveredIndex == index);

    painter->setRenderHint(QPainter::Antialiasing);

    const bool itemEnabled = (opt.state & QStyle::State_Enabled);
    const QPalette::ColorRole itemRole
        = (opt.state & QStyle::State_Selected) ? QPalette::HighlightedText : QPalette::Text;

    for(int i{0}; i < btnCount; ++i) {
        const QRect rect     = buttonRect(opt, i);
        const bool isHovered = isHoveredRow && m_hoveredButton == i;

        if(isHovered) {
            painter->setPen(opt.palette.color(QPalette::ButtonText));
            painter->setBrush(opt.palette.color(QPalette::Button));
            painter->drawRoundedRect(rect, ButtonRadius, ButtonRadius);
        }

        const auto textRole = isHovered ? QPalette::ButtonText : itemRole;
        style->drawItemText(painter, rect, Qt::AlignCenter, opt.palette, itemEnabled, btns[i].text, textRole);
    }

    painter->restore();
}

QSize ActionDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    size.setHeight(std::max(size.height(), ButtonSize + (2 * ButtonMargin)));
    return size;
}

bool ActionDelegate::helpEvent(QHelpEvent* event, QAbstractItemView* view, const QStyleOptionViewItem& option,
                               const QModelIndex& index)
{
    if(!event || !view) {
        return false;
    }

    QStyleOptionViewItem opt{option};
    initStyleOption(&opt, index);

    const auto btns     = buttons(index);
    const auto btnCount = static_cast<int>(btns.size());

    for(int i{0}; i < btnCount; ++i) {
        const QRect rect = buttonRect(opt, i);
        if(!rect.contains(event->pos())) {
            continue;
        }

        if(btns[i].tooltip.isEmpty()) {
            QToolTip::hideText();
            return true;
        }

        QToolTip::showText(event->globalPos(), btns[i].tooltip, view->viewport(), rect);
        return true;
    }

    return QStyledItemDelegate::helpEvent(event, view, option, index);
}

bool ActionDelegate::editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
                                 const QModelIndex& index)
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    const QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();

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

    const QPoint pos             = mouseEvent->position().toPoint();
    const QVariant checkStateVar = index.data(Qt::CheckStateRole);
    const bool hasCheck          = (index.flags() & Qt::ItemIsUserCheckable) && checkStateVar.isValid();

    if(hasCheck && model) {
        const QRect indicatorRect = checkRect(opt, style);
        if(indicatorRect.contains(pos)) {
            const auto currentState        = static_cast<Qt::CheckState>(checkStateVar.toInt());
            const Qt::CheckState nextState = (currentState == Qt::Checked) ? Qt::Unchecked : Qt::Checked;
            return model->setData(index, nextState, Qt::CheckStateRole);
        }
    }

    for(int i{0}; i < btnCount; ++i) {
        if(buttonRect(opt, i).contains(pos)) {
            Q_EMIT buttonClicked(index, btns[i].id);
            return true;
        }
    }

    return false;
}
} // namespace Fooyin
