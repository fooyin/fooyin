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

#include <gui/fywidget.h>

namespace Fooyin {
class SettingsManager;
class Track;

namespace WaveBar {
class WaveformBuilder;
class WaveSeekBar;

class WaveBarWidget : public FyWidget
{
    Q_OBJECT

public:
    WaveBarWidget(WaveformBuilder* builder, SettingsManager* settings, QWidget* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;

    void changeTrack(const Track& track);
    void changePosition(uint64_t pos);

signals:
    void seek(uint64_t pos);
    void seekForward();
    void seekBackward();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    SettingsManager* m_settings;

    WaveSeekBar* m_seekbar;
    WaveformBuilder* m_builder;
};
} // namespace WaveBar
} // namespace Fooyin
