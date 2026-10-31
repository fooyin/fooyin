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

#include <gui/widgets/backgroundconfig.h>

#include <QBasicTimer>
#include <QImage>
#include <QObject>
#include <QPixmap>

#include <cstdint>
#include <optional>

class QPainter;

namespace Fooyin {
class PixmapFadeController;

class BackgroundRenderer : public QObject
{
    Q_OBJECT

public:
    explicit BackgroundRenderer(QObject* parent);

    [[nodiscard]] bool hasImage() const;

    void setCanvas(const QSize& size, qreal dpr);
    void setConfig(const BackgroundConfig& config);
    void setPixmap(const QPixmap& pixmap);

    void draw(QPainter& painter);

Q_SIGNALS:
    void changed();

protected:
    void timerEvent(QTimerEvent* event) override;

private:
    struct PreparationKey
    {
        qint64 sourceKey{0};
        QSize size;
        qreal dpr{1.0};
        int blur{0};

        bool operator==(const PreparationKey& other) const = default;
    };

    struct PreparationRequest
    {
        PreparationKey key;
        QImage image;
        uint64_t generation{0};
    };

    struct PixmapCache
    {
        PreparationKey key;
        PreparationKey requestedKey;
        QPixmap pixmap;
        uint64_t generation{0};
        std::optional<PreparationRequest> pending;
    };

    void invalidateScaledBackground();
    void startPreparation();
    QPixmap preparedBackgroundPixmap(const QPixmap& source, PixmapCache& cache);
    [[nodiscard]] QPoint backgroundPixmapPosition(const QSize& pixmapSize) const;
    void drawBackgroundPixmap(QPainter& painter, const QPixmap& pixmap);

    QSize m_size;
    qreal m_dpr;
    BackgroundConfig m_config;
    PixmapFadeController* m_bgFadeController;
    QBasicTimer m_bgResizeTimer;
    PixmapCache m_cachedBg;
    PixmapCache m_cachedPreviousBg;
    bool m_preparing;
};
} // namespace Fooyin
