/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include <gui/widgets/tooltip.h>

#include <QPainter>

constexpr int TextMargin   = 5;
constexpr int BorderMargin = 2;

namespace Fooyin {
ToolTip::ToolTip(QWidget* parent)
    : QWidget{parent}
    , m_align{Qt::AlignLeft}
{
    setAttribute(Qt::WA_TransparentForMouseEvents);

    m_font.setBold(true);
    m_subFont.setPointSize(m_subFont.pointSize() - 2);
}

void ToolTip::setText(const QString& text)
{
    m_text = text;
    redraw();
}

void ToolTip::setSubtext(const QString& text)
{
    m_subText = text;
    redraw();
}

void ToolTip::setPosition(const QPoint& pos, Qt::Alignment align)
{
    m_pos   = pos;
    m_align = align;
}

void ToolTip::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p{this};
    p.drawPixmap(0, 0, m_pixmap);
}

void ToolTip::redraw()
{
    const QFontMetrics fm{m_font};
    const QFontMetrics subFm{m_subFont};

    const int textWidth = std::max(fm.horizontalAdvance(m_text), subFm.horizontalAdvance(m_subText));

    const QRect textRect{TextMargin, TextMargin, textWidth + TextMargin, fm.height()};
    const QRect subTextRect{TextMargin, m_text.isEmpty() ? TextMargin : textRect.bottom(), textWidth + TextMargin,
                            subFm.height()};

    const auto bottom = !m_subText.isEmpty() ? subTextRect.bottom() : textRect.bottom();

    const QRect rect{0, 0, textRect.right() + TextMargin, bottom + TextMargin};
    const QRect bgRect{0, 0, rect.width(), bottom + TextMargin};

    // Cache the tooltip background
    if(rect.size() != m_cachedBg.size()) {
        // TODO: Support custom colours
        const QColor rectColour{palette().highlight().color()};
        const QColor borderColour{rectColour.darker(140)};

        m_cachedBg = QPixmap{rect.size()};
        m_cachedBg.fill(Qt::transparent);

        QPainter p{&m_cachedBg};
        p.setRenderHint(QPainter::Antialiasing);

        p.setPen(Qt::NoPen);
        p.setBrush(borderColour);
        p.drawRect(bgRect);

        p.setBrush(rectColour);
        p.drawRect(bgRect.adjusted(BorderMargin, BorderMargin, -BorderMargin, -BorderMargin));
    }

    const qreal dpr = devicePixelRatioF();
    const auto size = rect.size() * dpr;
    m_pixmap        = QPixmap{size};
    m_pixmap.setDevicePixelRatio(dpr);
    m_pixmap.fill(Qt::transparent);

    QPainter p{&m_pixmap};
    p.setRenderHint(QPainter::Antialiasing);

    p.drawPixmap(rect.topLeft(), m_cachedBg);

    p.setPen(palette().highlightedText().color());
    p.setFont(m_font);
    p.drawText(textRect, Qt::AlignHCenter, m_text);

    p.setFont(m_subFont);
    p.drawText(subTextRect, Qt::AlignHCenter, m_subText);

    resize(m_pixmap.size());

    if(m_align == Qt::AlignLeft) {
        move(m_pos.x(), m_pos.y() - m_pixmap.height());
    }
    else if(m_align == Qt::AlignRight) {
        move(m_pos.x() - m_pixmap.width(), m_pos.y() - m_pixmap.height());
    }

    update();
}
} // namespace Fooyin

#include "gui/widgets/moc_tooltip.cpp"
