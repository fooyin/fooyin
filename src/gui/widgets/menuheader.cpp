/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include "menuheader.h"

#include <QApplication>
#include <QPainter>
#include <QStyleOptionMenuItem>

using namespace Qt::StringLiterals;

namespace {
QColor headerOverlayColour(const QPalette& palette)
{
    QColor colour = palette.text().color();
    colour.setAlpha(18);
    return colour;
}
} // namespace

namespace Fooyin {
MenuHeader::MenuHeader(QString text, QWidget* parent)
    : QWidget{parent}
    , m_minWidth{0}
    , m_text{std::move(text)}
    , m_textHeight{0}
    , m_margin{0}
{
    const QFontMetrics fm{fontMetrics()};
    m_textHeight = fm.height();
    m_margin     = fm.horizontalAdvance(u"..."_s);
    m_minWidth   = fm.boundingRect(m_text).width() + (3 * m_margin);

    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    updateGeometry();
}

QSize MenuHeader::minimumSizeHint() const
{
    return {m_minWidth, m_textHeight + m_margin};
}

QSize MenuHeader::sizeHint() const
{
    return {m_minWidth, m_textHeight + m_margin};
}

void MenuHeader::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter{this};

    QStyleOptionMenuItem option;
    option.initFrom(this);

    const QRect headerRect{0, 0, width(), m_textHeight + m_margin};
    const QColor headerBackground = option.palette.window().color();
    const QColor headerOverlay    = headerOverlayColour(option.palette);
    const QColor headerText       = option.palette.text().color();

    painter.fillRect(headerRect, headerBackground);
    painter.fillRect(headerRect, headerOverlay);

    QFont font{painter.font()};
    font.setBold(true);
    painter.setFont(font);

    painter.setPen(headerText);
    painter.drawText(QRect{m_margin, 0, m_minWidth - (2 * m_margin), height()}, Qt::AlignVCenter, m_text);
}

MenuHeaderAction::MenuHeaderAction(const QString& text, QObject* parent)
    : QWidgetAction{parent}
{
    auto* header = new MenuHeader(text);
    // Takes ownership
    setDefaultWidget(header);
}
} // namespace Fooyin

#include "moc_menuheader.cpp"
