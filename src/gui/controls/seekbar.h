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

#pragma once

#include <core/player/playerdefs.h>
#include <gui/fywidget.h>

namespace Fooyin {
class PlayerController;
class SeekContainer;
class SettingsManager;
class Track;
class TrackSlider;

class SeekBar : public FyWidget
{
    Q_OBJECT

public:
    SeekBar(PlayerController* playerController, SettingsManager* settings, QWidget* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void reset();
    void trackChanged(const Track& track);
    void setCurrentPosition(uint64_t pos) const;
    void stateChanged(PlayState state);

    PlayerController* m_playerController;
    SettingsManager* m_settings;

    SeekContainer* m_container;
    TrackSlider* m_slider;
    uint64_t m_max;
};
} // namespace Fooyin
