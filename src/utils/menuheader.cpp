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

#include "menuheader.h"

#include <QPainter>

namespace Fy::Utils {
MenuHeader::MenuHeader(QString text, QWidget* parent)
    : QWidget(parent)
    , m_minWidth(0)
    , m_text(std::move(text))
    , m_textHeight(0)
    , m_margin(0)
{
    const int textMinWidth = fontMetrics().boundingRect(m_text).width();
    m_textHeight           = fontMetrics().height();
    m_margin               = fontMetrics().horizontalAdvance("...");
    m_minWidth             = 2 * m_margin + textMinWidth;
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

void MenuHeader::paintEvent(QPaintEvent* e)
{
    Q_UNUSED(e)
    QPainter painter(this);
    const QPalette palette        = this->palette();
    const QColor headerBackground = palette.color(QPalette::AlternateBase);
    const QColor headerText       = palette.color(QPalette::Text);

    painter.setBrush(headerBackground);
    painter.setPen(Qt::NoPen);
    painter.drawRect(QRect(0, 0, width(), m_textHeight + m_margin));

    painter.setPen(headerText);
    painter.drawText(QPoint(m_margin, static_cast<int>(0.25 * m_margin) + m_textHeight), m_text);
    painter.end();
}

MenuHeaderAction::MenuHeaderAction(const QString& text, QObject* parent)
    : QWidgetAction{parent}
{
    auto* header = new MenuHeader(text);
    // Takes ownership
    setDefaultWidget(header);
}
} // namespace Fy::Utils
