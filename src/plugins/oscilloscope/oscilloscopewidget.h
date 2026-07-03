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

#include <core/engine/visualisationservice.h>
#include <core/player/playerdefs.h>
#include <gui/fywidget.h>

#include <QBasicTimer>
#include <QElapsedTimer>
#include <QVariant>

#include <array>

class QJsonObject;
class QPainter;
class QPalette;

namespace Fooyin {
class EngineController;
class PlayerController;
class SettingsManager;

namespace Oscilloscope {
class Colours;

class OscilloscopeWidget : public FyWidget
{
    Q_OBJECT

public:
    enum class DownmixMode : uint8_t
    {
        Stereo = 0,
        Mono
    };

    static constexpr int MinCurveDurationMs     = 25;
    static constexpr int MaxCurveDurationMs     = 1000;
    static constexpr int DefaultCurveDurationMs = 150;
    static constexpr std::array<int, 8> CurveDurationPresets{100, 200, 300, 400, 500, 600, 700, 800};
    static constexpr int MinZoomPercent     = 50;
    static constexpr int MaxZoomPercent     = 800;
    static constexpr int DefaultZoomPercent = 100;
    static constexpr int DefaultUpdateFps   = 30;
    static constexpr std::array<int, 9> ZoomPresets{50, 75, 100, 150, 200, 300, 400, 600, 800};

    struct ConfigData
    {
        int curveDurationMs{DefaultCurveDurationMs};
        int zoomPercent{DefaultZoomPercent};
        int updateFps{DefaultUpdateFps};
        DownmixMode downmixMode{DownmixMode::Stereo};
        bool showZeroLine{false};
        QVariant colours;
    };

    explicit OscilloscopeWidget(EngineController* engine, PlayerController* playerController, SettingsManager* settings,
                                QWidget* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;

    [[nodiscard]] ConfigData factoryConfig() const;
    [[nodiscard]] ConfigData defaultConfig() const;
    [[nodiscard]] const ConfigData& currentConfig() const;
    void saveDefaults(const ConfigData& config) const;
    void clearSavedDefaults() const;
    void applyConfig(const ConfigData& config);

    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void timerEvent(QTimerEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

    void openConfigDialog() override;

private:
    void handlePlayStateChanged(Player::PlayState state);
    void clearWindow(bool stopTimer);
    [[nodiscard]] int displayedLanes() const;
    [[nodiscard]] Colours colours() const;
    void updateSessionConfig();
    void startUpdateTimer();
    void settlePresentationClock();
    void tick();
    void paint(QPainter& painter, const QRect& rect, const QPalette& palette) const;
    [[nodiscard]] ConfigData configFromLayout(const QJsonObject& layout) const;
    void saveConfigToLayout(const ConfigData& config, QJsonObject& layout) const;

    SettingsManager* m_settings;
    VisualisationSessionPtr m_session;
    ConfigData m_config;
    VisualisationSession::PcmWindow m_window;
    bool m_active;
    bool m_paused;
    bool m_clockSettling;
    double m_presentationTimeMs;
    QBasicTimer m_updateTimer;
    QElapsedTimer m_presentationClock;
    QElapsedTimer m_clockSettleTimer;
};
} // namespace Oscilloscope
} // namespace Fooyin
