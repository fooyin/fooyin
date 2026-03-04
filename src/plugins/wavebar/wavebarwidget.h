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
#include <utils/database/dbconnectionpool.h>

#include <QVariant>

class QJsonObject;

namespace Fooyin {
class AudioLoader;
class PlayerController;
class SeekContainer;
class SettingsManager;
class Track;

namespace WaveBar {
class WaveformBuilder;
class WaveSeekBar;

class WaveBarWidget : public FyWidget
{
    Q_OBJECT

public:
    WaveBarWidget(std::shared_ptr<AudioLoader> audioLoader, DbConnectionPoolPtr dbPool,
                  PlayerController* playerController, SettingsManager* settings, QWidget* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;

    void changeTrack(const Track& track, bool update = false);

    struct ConfigData
    {
        bool showLabels{false};
        bool elapsedTotal{false};
        bool showCursor{true};
        int cursorWidth{3};
        int mode{3};
        int downmix{0};
        int barWidth{1};
        int barGap{0};
        double maxScale{1.0};
        int centreGap{0};
        double channelScale{0.9};
        QVariant colourOptions;
    };

    [[nodiscard]] ConfigData defaultConfig() const;
    [[nodiscard]] const ConfigData& currentConfig() const;
    void saveDefaults(const ConfigData& config) const;
    void applyConfig(const ConfigData& config);

    [[nodiscard]] int globalNumSamples() const;
    bool setGlobalNumSamples(int samples) const;
    [[nodiscard]] QString cacheSizeText() const;
    void requestClearCache();

signals:
    void clearCacheRequested();

protected:
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    [[nodiscard]] ConfigData configFromLayout(const QJsonObject& layout) const;
    void saveConfigToLayout(const ConfigData& config, QJsonObject& layout) const;
    void openConfigDialog() override;

    void rescaleWaveform();

    PlayerController* m_playerController;
    SettingsManager* m_settings;

    SeekContainer* m_container;
    WaveSeekBar* m_seekbar;
    std::unique_ptr<WaveformBuilder> m_builder;
    ConfigData m_config;
};
} // namespace WaveBar
} // namespace Fooyin
