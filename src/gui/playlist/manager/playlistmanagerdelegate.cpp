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

#include "playlistmanagerdelegate.h"

#include <gui/guiutils.h>

#include <QApplication>
#include <QPainter>
#include <QStyle>
#include <QWidget>

namespace Fooyin {
void PlaylistManagerDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                    const QModelIndex& index) const
{
    QStyleOptionViewItem opt{option};
    initStyleOption(&opt, index);

    const QStyle* style  = opt.widget ? opt.widget->style() : QApplication::style();
    const QRect textRect = Gui::itemViewTextRect(opt);
    const QString text   = opt.fontMetrics.elidedText(opt.text, opt.textElideMode, textRect.width());
    const auto textRole
        = opt.state.testFlag(QStyle::State_Selected) ? Gui::itemViewSelectionTextRole(opt) : QPalette::Text;

    if(!opt.icon.isNull()) {
        const QRect decorationRect = style->subElementRect(QStyle::SE_ItemViewItemDecoration, &opt, opt.widget);
        const QIcon::Mode mode     = opt.state.testFlag(QStyle::State_Enabled) ? QIcon::Normal : QIcon::Disabled;
        const QIcon::State state   = opt.state.testFlag(QStyle::State_Open) ? QIcon::On : QIcon::Off;
        opt.icon.paint(painter, decorationRect, opt.decorationAlignment, mode, state);
    }

    style->drawItemText(painter, textRect, opt.displayAlignment, opt.palette, opt.state.testFlag(QStyle::State_Enabled),
                        text, textRole);
}

void PlaylistManagerDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
                                                   const QModelIndex& index) const
{
    if(!editor) {
        return;
    }

    QStyleOptionViewItem opt{option};
    initStyleOption(&opt, index);

    editor->setGeometry(Gui::itemViewTextRect(opt));
}
} // namespace Fooyin
