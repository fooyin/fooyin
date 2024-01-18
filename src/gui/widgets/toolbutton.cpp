/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

#include <gui/widgets/toolbutton.h>

#include <QStyleOptionToolButton>
#include <QStylePainter>

namespace Fooyin {
ToolButton::ToolButton(QWidget* parent)
    : QToolButton{parent}
    , m_padding{0}
    , m_minimumSize{10}
    , m_maximumSize{20}
{ }

void ToolButton::setStretchEnabled(bool enabled)
{
    setSizePolicy(enabled ? QSizePolicy::Expanding : QSizePolicy::Preferred, QSizePolicy::Preferred);
}

void ToolButton::setIconPadding(int padding)
{
    m_padding = padding;
}

void ToolButton::setMinimumIconSize(int size)
{
    m_minimumSize = size;
}

void ToolButton::setMaximumIconSize(int size)
{
    m_maximumSize = size;
}

void ToolButton::enterEvent(QEnterEvent* event)
{
    QToolButton::enterEvent(event);
    emit entered();
}

void ToolButton::paintEvent(QPaintEvent* /*event*/)
{
    QStylePainter painter{this};
    QStyleOptionToolButton opt;
    initStyleOption(&opt);

    // Remove menu indicator
    opt.features &= ~QStyleOptionToolButton::HasMenu;

    const QRect& rect  = opt.rect;
    const int baseSize = std::min(rect.height(), rect.width()) - (2 * m_padding);
    const int maxSize  = std::clamp(baseSize, m_minimumSize, m_maximumSize);
    opt.iconSize       = {maxSize, maxSize};

    painter.drawComplexControl(QStyle::CC_ToolButton, opt);
}
} // namespace Fooyin

#include "gui/widgets/moc_toolbutton.cpp"
