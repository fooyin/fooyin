/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <LukeT1@proton.me>
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

#include "internalguisettings.h"

#include <core/track.h>
#include <gui/fywidget.h>

#include <QBasicTimer>
#include <QPixmap>

namespace Fooyin {
class CoverProvider;
class AudioLoader;
class PlayerController;
class SettingsManager;
class TrackSelectionController;

class CoverWidget : public FyWidget
{
    Q_OBJECT

public:
    explicit CoverWidget(PlayerController* playerController, TrackSelectionController* trackSelection,
                         std::shared_ptr<AudioLoader> audioLoader, SettingsManager* settings,
                         QWidget* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;

    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;

signals:
    void requestArtworkSearch(const Fooyin::TrackList& tracks, Fooyin::Track::Cover type);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void timerEvent(QTimerEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void rescaleCover();
    void reloadCover();

    PlayerController* m_playerController;
    TrackSelectionController* m_trackSelection;
    SettingsManager* m_settings;
    CoverProvider* m_coverProvider;

    SelectionDisplay m_displayOption;
    Track::Cover m_coverType;
    Qt::Alignment m_coverAlignment;
    bool m_keepAspectRatio;
    QBasicTimer m_resizeTimer;

    Track m_track;
    QPixmap m_cover;
    QPixmap m_scaledCover;
};
} // namespace Fooyin
