/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <luket@pm.me>
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

#include <core/track.h>
#include <gui/coverartworktypes.h>
#include <gui/fywidget.h>
#include <gui/trackdisplay.h>

#include <QBasicTimer>
#include <QElapsedTimer>
#include <QPixmap>

namespace Fooyin {
class ActionManager;
class AudioLoader;
class CoverProvider;
class CoverRepository;
class PixmapFadeController;
class PlaylistHandler;
class PlayerController;
class SettingsManager;
class TrackSelectionController;

enum class CoverAction : uint8_t
{
    None = 0,
    ViewFullSize,
    OpenContainingFolder,
    NextArtworkType,
    ShowTrack,
    OpenProperties,
};

class CoverWidget : public FyWidget
{
    Q_OBJECT

public:
    static constexpr int MaxSpinSpeedRpm       = 60;
    static constexpr int MaxSpinRampDurationMs = 10000;

    struct ConfigData
    {
        Track::Cover coverType{Track::Cover::Front};
        Qt::Alignment coverAlignment{Qt::AlignCenter};
        ArtworkScaling artworkScaling{ArtworkScaling::Fit};
        CoverStyle style{CoverStyle::Cover};
        bool spinDisc{true};
        int spinSpeedRpm{6};
        int spinRampDurationMs{1000};
        TrackDisplayPreference trackPreference{TrackDisplayPreference::PlayingTrack};
        bool fadeCoverChanges{false};
        int fadeDurationMs{1000};
        CoverAction doubleClickAction{CoverAction::ViewFullSize};
        CoverAction middleClickAction{CoverAction::NextArtworkType};
    };

    explicit CoverWidget(ActionManager* actionManager, PlayerController* playerController,
                         PlaylistHandler* playlistHandler, TrackSelectionController* trackSelection,
                         std::shared_ptr<AudioLoader> audioLoader, CoverRepository* coverRepository,
                         SettingsManager* settings, QWidget* parent = nullptr);

    [[nodiscard]] ConfigData factoryConfig() const;
    [[nodiscard]] ConfigData defaultConfig() const;
    [[nodiscard]] const ConfigData& currentConfig() const;
    void applyConfig(const ConfigData& config);
    void saveDefaults(const ConfigData& config) const;
    void clearSavedDefaults() const;
    [[nodiscard]] ConfigData configFromLayout(const QJsonObject& layout) const;
    static void saveConfigToLayout(const ConfigData& config, QJsonObject& layout);

    void rescaleCover();
    void reloadCover();

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;

    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;

Q_SIGNALS:
    void configChanged();
    void requestPropertiesDialog(const Fooyin::TrackList& tracks);
    void requestArtworkSearch(const Fooyin::TrackList& tracks, Fooyin::Track::Cover type, bool quick);
    void requestArtworkRemoval(const Fooyin::TrackList& tracks, Fooyin::Track::Cover type);

protected:
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void timerEvent(QTimerEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void openConfigDialog() override;

private:
    [[nodiscard]] QPixmap effectiveCover(const QPixmap& cover) const;
    [[nodiscard]] QPixmap scaledCover(const QPixmap& cover) const;
    [[nodiscard]] qreal discAngle() const;
    [[nodiscard]] qreal discSpeedRpm() const;

    void updateDiscRotation();

    [[nodiscard]] Track displayTrack() const;
    [[nodiscard]] static bool sameDisplayTrack(const Track& lhs, const Track& rhs);

    void setFadeCoverChanges(bool enabled);
    void stopCoverFade();
    void setCoverPixmap(const QPixmap& cover, bool forceRescale = false);
    void updatePlaceholderCover();
    void handleSelectionChanged();

    void performAction(CoverAction action);
    void showArtworkViewer();
    void openContainingFolder() const;
    void showTrack() const;
    void showNextArtworkType();
    void tryArtworkType(const Track& track, Track::Cover type, int remaining, int requestId);
    void checkTrackArtwork(const Track& track);

    ActionManager* m_actionManager;
    PlayerController* m_playerController;
    PlaylistHandler* m_playlistHandler;
    TrackSelectionController* m_trackSelection;
    std::shared_ptr<AudioLoader> m_audioLoader;
    SettingsManager* m_settings;
    CoverProvider* m_coverProvider;

    ConfigData m_config;
    Track::Cover m_coverType;
    Qt::Alignment m_coverAlignment;
    ArtworkScaling m_artworkScaling;

    QBasicTimer m_resizeTimer;
    QBasicTimer m_rotationTimer;
    QElapsedTimer m_rotationElapsed;
    qreal m_discAngle;
    qreal m_spinStartSpeedRpm;
    qreal m_spinTargetSpeedRpm;

    bool m_fadeCoverChanges;
    PixmapFadeController* m_fadeController;
    int m_coverRequestId;
    int m_actionRequestId;

    Track m_track;
    QPixmap m_cover;
    QPixmap m_scaledCover;
    QPixmap m_previousScaledCover;
    QPixmap m_noCover;
};
} // namespace Fooyin
