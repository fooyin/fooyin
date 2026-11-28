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

#include "theme/progressbarappearance.h"

#include <QBasicTimer>
#include <QElapsedTimer>
#include <QPointer>
#include <QStyle>

#include <vector>

class QProgressBar;
class QStyleOptionProgressBar;

namespace Fooyin::Style::Progress {
class Animation : public QObject
{
public:
    explicit Animation(QStyle* style);

    double phase(const QStyleOptionProgressBar& option, const QWidget* widget, const ProgressBarAppearance& appearance);
    void remove(const QWidget* widget);

protected:
    void timerEvent(QTimerEvent* event) override;

private:
    QBasicTimer m_timer;
    QElapsedTimer m_elapsed;
    std::vector<QPointer<QProgressBar>> m_bars;
};

void draw(QStyle::ControlElement element, const QStyleOptionProgressBar& option, QPainter& painter, double phase,
          const ProgressBarAppearance& appearance);
QRect subElementRect(QStyle::SubElement element, const QStyleOptionProgressBar& option,
                     const ProgressBarAppearance& appearance);
QSize sizeFromContents(const QStyleOptionProgressBar& option, const QSize& contentsSize,
                       const ProgressBarAppearance& appearance);
} // namespace Fooyin::Style::Progress
