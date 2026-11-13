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

#include <QObject>
#include <QPixmap>

#include <functional>

class QVariantAnimation;

namespace Fooyin {
class PixmapFadeController : public QObject
{
    Q_OBJECT

public:
    static constexpr int MinDurationMs = 50;
    static constexpr int MaxDurationMs = 3000;

    explicit PixmapFadeController(QObject* parent = nullptr);

    void setUpdateCallback(std::function<void()> callback);
    void setEnabled(bool enabled);
    void setDurationMs(int durationMs);
    bool setPixmap(const QPixmap& pixmap);
    void stop();

    [[nodiscard]] bool enabled() const;
    [[nodiscard]] int durationMs() const;
    [[nodiscard]] qreal progress() const;
    [[nodiscard]] const QPixmap& pixmap() const;
    [[nodiscard]] const QPixmap& previousPixmap() const;

    [[nodiscard]] static bool pixmapsMatch(const QPixmap& lhs, const QPixmap& rhs);

private:
    void requestUpdate();

    QVariantAnimation* m_animation;
    std::function<void()> m_updateCallback;
    bool m_enabled;
    int m_durationMs;
    qreal m_progress;
    QPixmap m_pixmap;
    QPixmap m_prevPixmap;
};
} // namespace Fooyin
