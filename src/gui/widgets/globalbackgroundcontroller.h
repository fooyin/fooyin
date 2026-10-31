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

class QPainter;
class QWidget;

namespace Fooyin {
class BackgroundImageSource;
class BackgroundRenderer;
class CoverProvider;
class PlayerController;
class SettingsManager;
class TrackSelectionController;

class GlobalBackgroundController : public QObject
{
    Q_OBJECT

public:
    GlobalBackgroundController(QWidget* window, CoverProvider* coverProvider, PlayerController* playerController,
                               TrackSelectionController* selectionController, SettingsManager* settings);

    static GlobalBackgroundController* forWidget(const QWidget* widget);

    [[nodiscard]] bool hasImage() const;

    void draw(QPainter& painter, const QWidget* viewport);

    bool eventFilter(QObject* watched, QEvent* event) override;

Q_SIGNALS:
    void changed();

private:
    QWidget* m_window;
    BackgroundImageSource* m_source;
    BackgroundRenderer* m_renderer;
    bool m_useSharedBase;
};
} // namespace Fooyin
