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

#include "layoutdragcard.h"

#include <gui/guiconstants.h>
#include <gui/iconloader.h>

#include <QGraphicsDropShadowEffect>
#include <QPainter>
#include <QRect>

namespace Fooyin {
LayoutDragCard::LayoutDragCard(QWidget* parent)
    : QWidget{parent}
    , m_icon{Gui::iconFromTheme(Constants::Icons::Add)}
    , m_headerHeight{36}
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(16);
    shadow->setColor(QColor{0, 0, 0, 100});
    shadow->setOffset(1, 2);

    setGraphicsEffect(shadow);
    hide();
}

void LayoutDragCard::setContent(const QString& text, bool creating, QPixmap snapshot)
{
    m_text     = text;
    m_icon     = Gui::iconFromTheme(creating ? Constants::Icons::Add : Constants::Icons::LayoutEditing);
    m_snapshot = std::move(snapshot);

    if(!m_snapshot.isNull()) {
        const auto logicalSize = m_snapshot.deviceIndependentSize().toSize();
        if(logicalSize.width() > 240 || logicalSize.height() > 160) {
            const auto size  = logicalSize.scaled(240, 160, Qt::KeepAspectRatio);
            const auto ratio = m_snapshot.devicePixelRatio();
            m_snapshot       = m_snapshot.scaled(size * ratio, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            m_snapshot.setDevicePixelRatio(ratio);
        }
    }

    m_headerHeight         = std::max(36, fontMetrics().height() + 16);
    const auto previewSize = m_snapshot.deviceIndependentSize().toSize();
    resize(std::max(std::min(280, fontMetrics().horizontalAdvance(text) + 52), previewSize.width() + 16),
           m_headerHeight + (m_snapshot.isNull() ? 0 : previewSize.height() + 8));

    update();
}

void LayoutDragCard::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter{this};
    painter.setRenderHint(QPainter::Antialiasing);

    auto background = palette().color(QPalette::Window);
    background.setAlpha(255);

    painter.setBrush(background);
    painter.setPen(palette().color(QPalette::Mid));

    painter.drawRoundedRect(QRectF{rect()}.adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
    m_icon.paint(&painter, QRect{10, (m_headerHeight - 20) / 2, 20, 20});

    painter.setPen(palette().color(QPalette::WindowText));
    painter.drawText(QRect{38, 0, width() - 48, m_headerHeight}, Qt::AlignVCenter | Qt::AlignLeft,
                     fontMetrics().elidedText(m_text, Qt::ElideRight, width() - 48));

    if(!m_snapshot.isNull()) {
        const auto previewSize = m_snapshot.deviceIndependentSize().toSize();
        painter.drawPixmap(
            QRect{(width() - previewSize.width()) / 2, m_headerHeight, previewSize.width(), previewSize.height()},
            m_snapshot, m_snapshot.rect());
    }
}
} // namespace Fooyin
