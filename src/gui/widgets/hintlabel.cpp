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

#include <gui/widgets/hintlabel.h>

#include <gui/guiconstants.h>
#include <gui/iconloader.h>

#include <QEvent>
#include <QPainter>
#include <QStyle>
#include <QStyleOption>

namespace Fooyin {
HintLabel::HintLabel(QWidget* parent)
    : HintLabel{QString{}, parent}
{ }

HintLabel::HintLabel(const QString& text, QWidget* parent)
    : HintLabel{text, Type::Information, parent}
{ }

HintLabel::HintLabel(const QString& text, Type type, QWidget* parent)
    : QLabel{text, parent}
{
    setWordWrap(true);
    setAlignment(Qt::AlignLeft | Qt::AlignTop);
    setType(type);
    updateMargins();
}

void HintLabel::setType(Type type)
{
    const auto* iconName = type == Type::Warning ? Constants::Icons::Warning : Constants::Icons::Information;
    m_icon               = Gui::iconFromTheme(iconName);
    update();
}

void HintLabel::changeEvent(QEvent* event)
{
    QLabel::changeEvent(event);

    switch(event->type()) {
        case QEvent::FontChange:
        case QEvent::StyleChange:
        case QEvent::LayoutDirectionChange:
            updateMargins();
            break;
        default:
            break;
    }
}

void HintLabel::paintEvent(QPaintEvent* event)
{
    QLabel::paintEvent(event);

    if(text().isEmpty()) {
        return;
    }

    int top{0};
    if(alignment() & Qt::AlignVCenter) {
        top = (height() - heightForWidth(width())) / 2;
    }
    else if(alignment() & Qt::AlignBottom) {
        top = height() - heightForWidth(width());
    }

    const int height     = fontMetrics().height();
    const QRect iconRect = QStyle::visualRect(layoutDirection(), rect(), QRect{0, top, height, height});

    QStyleOption option;
    option.initFrom(this);

    QPixmap pixmap = m_icon.pixmap(QSize{height, height}, devicePixelRatioF());
    if(pixmap.isNull()) {
        return;
    }

    QPainter iconPainter{&pixmap};
    iconPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    iconPainter.fillRect(pixmap.rect(), option.palette.color(foregroundRole()));
    iconPainter.end();

    QPainter painter{this};
    painter.drawPixmap(iconRect, pixmap);
}

void HintLabel::updateMargins()
{
    const QFontMetrics fm{fontMetrics()};
    const int leading = fm.height() + fm.horizontalAdvance(u' ');
    setContentsMargins(isRightToLeft() ? 0 : leading, 0, isRightToLeft() ? leading : 0, 0);
}
} // namespace Fooyin

#include "gui/widgets/moc_hintlabel.cpp"
