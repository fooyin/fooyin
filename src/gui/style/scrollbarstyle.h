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

#pragma once

#include "theme/scrollbarappearance.h"

#include <QBasicTimer>
#include <QElapsedTimer>
#include <QPointer>
#include <QStyle>

#include <optional>
#include <vector>

class QAbstractScrollArea;
class QScrollBar;
class QStyleOptionSlider;

namespace Fooyin {
struct ScrollBarAppearance;

namespace Style::ScrollBar {
class Animation : public QObject
{
public:
    Animation(QStyle* style, const ScrollBarAppearance& appearance);
    ~Animation() override;

    double opacity(const QStyleOptionSlider& option, const QWidget* widget);
    void polish(QWidget* widget);
    void remove(const QWidget* widget, bool restore = true);

    bool eventFilter(QObject* watched, QEvent* event) override;

protected:
    void timerEvent(QTimerEvent* event) override;

private:
    struct Entry
    {
        QPointer<QWidget> bar;
        QPointer<QWidget> area;
        QPointer<QWidget> viewport;
        QPointer<QWidget> container;
        qint64 lastActivity;
        double opacity;
        bool hovered;
        bool atEdge;
        bool hoverAttribute;
        bool opaqueAttribute;
        bool transparentAttribute;
        bool autoFillBackground;
        bool containerTransparent;
    };

    void reveal(const QWidget* area);
    void setOpacity(Entry& entry, double opacity);

    const ScrollBarAppearance::AutoHide m_settings;
    QBasicTimer m_timer;
    QElapsedTimer m_elapsed;
    std::vector<Entry> m_entries;
};

bool isTransient(const ScrollBarAppearance& appearance, const QWidget* widget);

void draw(const QStyle& style, const ScrollBarAppearance& appearance, const QStyleOptionSlider& option,
          QPainter& painter, const QWidget* widget);
void drawCorner(const ScrollBarAppearance& appearance, const QStyleOption& option, QPainter& painter);
std::optional<int> pixelMetric(QStyle::PixelMetric metric, const ScrollBarAppearance& appearance);
std::optional<int> styleHint(QStyle::StyleHint hint, const ScrollBarAppearance& appearance, const QWidget* widget);
QRect subControlRect(const QStyle& style, const QStyleOptionSlider& option, QStyle::SubControl subControl,
                     const QWidget* widget);
} // namespace Style::ScrollBar
} // namespace Fooyin
