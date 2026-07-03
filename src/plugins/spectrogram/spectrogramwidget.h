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

#include <core/engine/enginedefs.h>
#include <core/engine/visualisationservice.h>
#include <core/player/playerdefs.h>
#include <core/track.h>
#include <gui/fywidget.h>

#include <QBasicTimer>
#include <QColor>
#include <QElapsedTimer>
#include <QImage>
#include <QTimer>

#include <array>
#include <vector>

class QContextMenuEvent;
class QJsonObject;
class QPaintEvent;
class QResizeEvent;
class QTimerEvent;

namespace Fooyin {
class EngineController;
class PlayerController;
class SettingsManager;
} // namespace Fooyin

namespace Fooyin::Spectrogram {
class SpectrogramWidget : public FyWidget
{
    Q_OBJECT

public:
    static constexpr std::array<int, 7> FftSizes{256, 512, 1024, 2048, 4096, 8192, 16384};

    enum class ChannelMode : uint8_t
    {
        Unchanged = 0,
        Mono,
        FrontOnly,
        BackOnly,
    };

    enum class PresentationMode : uint8_t
    {
        Scrolling = 0,
        Stationary,
    };

    struct ConfigData
    {
        ChannelMode channelMode{ChannelMode::Unchanged};
        PresentationMode presentationMode{PresentationMode::Scrolling};
        bool logFrequency{true};
        bool clearOnTrackChange{false};
        int channelSpacing{0};
        int fftSize{8192};
        int minDb{-70};
        int maxDb{0};
        int windowFunction{static_cast<int>(VisualisationSession::SpectrumWindowFunction::BlackmanHarris)};
        std::vector<QColor> colours;
    };

    SpectrogramWidget(PlayerController* playerController, EngineController* engine, SettingsManager* settings,
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
    [[nodiscard]] QSize sizeHint() const override;

protected:
    void resizeEvent(QResizeEvent* event) override;
    void timerEvent(QTimerEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

    void openConfigDialog() override;

private:
    [[nodiscard]] ConfigData configFromLayout(const QJsonObject& layout) const;
    void saveConfigToLayout(const ConfigData& config, QJsonObject& layout) const;

    void updateTimerState();
    void resetAnalysis();
    void resizeBuffers();
    void normaliseHistoryForPresentationMode();
    void syncStreams();
    void setTrack(const Track& track);
    void setEngineState(Engine::PlaybackState state);
    void setPauseDrainActive(bool active);
    void handlePositionMoved(uint64_t positionMs);
    void handlePositionChanged(uint64_t positionMs);
    void renderFrame();
    bool analyseColumn(uint64_t endTimeMs);
    [[nodiscard]] QImage linearHistoryImage() const;
    [[nodiscard]] double oldestColumnTimeMs() const;

    PlayerController* m_playerController;
    VisualisationService* m_visualisationService;
    SettingsManager* m_settings;

    VisualisationSessionPtr m_monoSession;
    VisualisationSessionPtr m_leftSession;
    VisualisationSessionPtr m_rightSession;

    Track m_track;
    ConfigData m_config;
    QBasicTimer m_timer;

    QImage m_image;
    QImage m_resizeSource;
    int m_resizeSourceWritePixel;
    int m_resizeSourceHistoryPixelCount;
    QTimer m_resizeSettleTimer;
    qreal m_imageDpr;

    double m_pixelAdvanceRemainder;
    int m_writePixel;
    int m_historyPixelCount;
    int m_historyColumnCount;
    double m_newestColumnTimeMs;

    Player::PlayState m_playState;
    Engine::PlaybackState m_engineState;

    bool m_pauseDrainActive;
    uint64_t m_lastAnalysisTimeMs;
    bool m_analysisTimelineReady;
    uint64_t m_lastObservedAudioTimeMs;
    QElapsedTimer m_presentationClock;
    double m_presentationTimeMs;

    QElapsedTimer m_seekSettleClock;
    bool m_seekPending;
    bool m_seekAwaitingPosition;
    uint64_t m_requestedSeekPositionMs;
};
} // namespace Fooyin::Spectrogram
