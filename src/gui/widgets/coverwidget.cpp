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

#include "coverwidget.h"

#include "artwork/artworkexporter.h"
#include "artwork/artworkviewerdialog.h"
#include "coverwidgetconfigwidget.h"
#include "internalguisettings.h"
#include "pixmapfadecontroller.h"

#include <core/engine/audioloader.h>
#include <core/player/playercontroller.h>
#include <core/playlist/playlisthandler.h>
#include <core/track.h>
#include <gui/coverprovider.h>
#include <gui/coverrepository.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/guiutils.h>
#include <gui/statusevent.h>
#include <gui/trackselectioncontroller.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/fileutils.h>
#include <utils/settings/settingsdialogcontroller.h>
#include <utils/settings/settingsmanager.h>

#include <QAction>
#include <QActionGroup>
#include <QConicalGradient>
#include <QContextMenuEvent>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QMenu>
#include <QMouseEvent>
#include <QPainterPath>
#include <QRadialGradient>
#include <QStylePainter>
#include <QTimer>
#include <QTimerEvent>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;

constexpr auto CoverWidgetCoverTypeKey        = u"ArtworkPanel/CoverType";
constexpr auto CoverWidgetCoverAlignmentKey   = u"ArtworkPanel/CoverAlignment";
constexpr auto CoverWidgetKeepAspectRatioKey  = u"ArtworkPanel/KeepAspectRatio";
constexpr auto CoverWidgetArtworkScalingKey   = u"ArtworkPanel/ArtworkScaling";
constexpr auto CoverWidgetStyleKey            = u"ArtworkPanel/Style";
constexpr auto CoverWidgetSpinDiscKey         = u"ArtworkPanel/SpinDisc";
constexpr auto CoverWidgetSpinSpeedKey        = u"ArtworkPanel/SpinSpeedRpm";
constexpr auto CoverWidgetSpinRampDurationKey = u"ArtworkPanel/SpinRampDurationMs";
constexpr auto CoverWidgetTrackPreferenceKey  = u"ArtworkPanel/TrackPreference";
constexpr auto CoverWidgetShowStoppedTrackKey = u"ArtworkPanel/ShowStoppedTrack";
constexpr auto CoverWidgetFadeEnabledKey      = u"ArtworkPanel/FadeCoverChanges";
constexpr auto CoverWidgetFadeDurationKey     = u"ArtworkPanel/FadeDurationMs";
constexpr auto CoverWidgetDoubleClickKey      = u"ArtworkPanel/DoubleClickAction";
constexpr auto CoverWidgetMiddleClickKey      = u"ArtworkPanel/MiddleClickAction";

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
constexpr auto ResizeInterval   = 5ms;
constexpr auto RotationInterval = 16ms;
#else
constexpr auto ResizeInterval   = 5;
constexpr auto RotationInterval = 16;
#endif

namespace Fooyin {
CoverWidget::CoverWidget(ActionManager* actionManager, PlayerController* playerController,
                         PlaylistHandler* playlistHandler, TrackSelectionController* trackSelection,
                         std::shared_ptr<AudioLoader> audioLoader, CoverRepository* coverRepository,
                         SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_actionManager{actionManager}
    , m_playerController{playerController}
    , m_playlistHandler{playlistHandler}
    , m_trackSelection{trackSelection}
    , m_audioLoader{std::move(audioLoader)}
    , m_settings{settings}
    , m_coverProvider{new CoverProvider(coverRepository, this)}
    , m_coverType{Track::Cover::Front}
    , m_artworkScaling{ArtworkScaling::Fit}
    , m_discAngle{0.0}
    , m_spinStartSpeedRpm{0.0}
    , m_spinTargetSpeedRpm{0.0}
    , m_fadeCoverChanges{false}
    , m_fadeController{new PixmapFadeController(this)}
    , m_coverRequestId{0}
    , m_actionRequestId{0}
    , m_noCover{m_coverProvider->placeholderCover(m_coverType)}
{
    setObjectName(CoverWidget::name());

    m_coverProvider->setUsePlaceholder(false);

    m_fadeController->setUpdateCallback([this] { update(); });

    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this, &CoverWidget::reloadCover);
    QObject::connect(m_playerController, &PlayerController::currentTrackUpdated, this, &CoverWidget::reloadCover);
    QObject::connect(m_playerController, &PlayerController::playStateChanged, this, &CoverWidget::reloadCover);
    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this, &CoverWidget::checkTrackArtwork);
    QObject::connect(m_playlistHandler, &PlaylistHandler::playlistsPopulated, this, &CoverWidget::reloadCover);
    QObject::connect(m_playlistHandler, &PlaylistHandler::activePlaylistChanged, this, &CoverWidget::reloadCover);
    QObject::connect(m_trackSelection, &TrackSelectionController::displaySelectionChanged, this,
                     &CoverWidget::handleSelectionChanged);
    QObject::connect(m_coverProvider, &CoverProvider::coverAdded, this, &CoverWidget::reloadCover,
                     Qt::QueuedConnection);
    QObject::connect(m_coverProvider, &CoverProvider::placeholderChanged, this, [this]() {
        m_noCover = m_coverProvider->placeholderCover(m_coverType);
        reloadCover();
    });

    m_settings->subscribe<Settings::Gui::IconTheme>(this, &CoverWidget::reloadCover);
    m_settings->subscribe<Settings::Gui::ResolvedAppStyle>(this, &CoverWidget::reloadCover);

    applyConfig(defaultConfig());
    reloadCover();
}

CoverWidget::ConfigData CoverWidget::factoryConfig() const
{
    return {};
}

CoverWidget::ConfigData CoverWidget::defaultConfig() const
{
    auto config{factoryConfig()};

    config.coverType = static_cast<Track::Cover>(
        m_settings->fileValue(CoverWidgetCoverTypeKey, static_cast<int>(config.coverType)).toInt());
    config.coverAlignment = static_cast<Qt::Alignment>(
        m_settings->fileValue(CoverWidgetCoverAlignmentKey, static_cast<int>(config.coverAlignment)).toInt());
    config.style = m_settings->fileValue(CoverWidgetStyleKey, static_cast<int>(config.style)).toInt()
                        == static_cast<int>(CoverStyle::Disc)
                     ? CoverStyle::Disc
                     : CoverStyle::Cover;

    if(m_settings->fileContains(CoverWidgetArtworkScalingKey)) {
        config.artworkScaling = artworkScalingFromInt(m_settings->fileValue(CoverWidgetArtworkScalingKey).toInt());
    }
    else {
        config.artworkScaling = m_settings->fileValue(CoverWidgetKeepAspectRatioKey, true).toBool()
                                  ? ArtworkScaling::Fit
                                  : ArtworkScaling::Stretch;
    }

    if(m_settings->fileContains(CoverWidgetTrackPreferenceKey)) {
        config.trackPreference = static_cast<TrackDisplayPreference>(
            m_settings->fileValue(CoverWidgetTrackPreferenceKey, static_cast<int>(config.trackPreference)).toInt());
    }
    else {
        const bool preferSelection  = m_settings->fileValue(u"Artwork/DisplayOption", 0).toInt() == 1;
        const bool showStoppedTrack = m_settings->fileValue(CoverWidgetShowStoppedTrackKey, true).toBool();
        config.trackPreference      = preferSelection  ? TrackDisplayPreference::SelectedTrack
                                    : showStoppedTrack ? TrackDisplayPreference::PlayingTrack
                                                       : TrackDisplayPreference::PlayingTrackBlankWhenStopped;
    }

    config.fadeCoverChanges = m_settings->fileValue(CoverWidgetFadeEnabledKey, config.fadeCoverChanges).toBool();
    config.spinDisc         = m_settings->fileValue(CoverWidgetSpinDiscKey, config.spinDisc).toBool();
    config.spinSpeedRpm
        = std::clamp(m_settings->fileValue(CoverWidgetSpinSpeedKey, config.spinSpeedRpm).toInt(), 0, MaxSpinSpeedRpm);
    config.spinRampDurationMs
        = std::clamp(m_settings->fileValue(CoverWidgetSpinRampDurationKey, config.spinRampDurationMs).toInt(), 0,
                     MaxSpinRampDurationMs);
    config.fadeDurationMs    = m_settings->fileValue(CoverWidgetFadeDurationKey, config.fadeDurationMs).toInt();
    config.doubleClickAction = static_cast<CoverAction>(
        m_settings->fileValue(CoverWidgetDoubleClickKey, static_cast<int>(config.doubleClickAction)).toInt());
    config.middleClickAction = static_cast<CoverAction>(
        m_settings->fileValue(CoverWidgetMiddleClickKey, static_cast<int>(config.middleClickAction)).toInt());
    config.fadeDurationMs
        = std::clamp(config.fadeDurationMs, PixmapFadeController::MinDurationMs, PixmapFadeController::MaxDurationMs);

    return config;
}

const CoverWidget::ConfigData& CoverWidget::currentConfig() const
{
    return m_config;
}

void CoverWidget::applyConfig(const ConfigData& config)
{
    const bool coverTypeChanged       = m_coverType != config.coverType;
    const auto artworkScaling         = artworkScalingFromInt(static_cast<int>(config.artworkScaling));
    const bool scalingChanged         = m_artworkScaling != artworkScaling;
    const auto style                  = config.style == CoverStyle::Disc ? CoverStyle::Disc : CoverStyle::Cover;
    const bool styleChanged           = m_config.style != style;
    const bool alignmentChanged       = m_coverAlignment != config.coverAlignment;
    const bool fadeEnabledChanged     = m_fadeCoverChanges != config.fadeCoverChanges;
    const bool trackPreferenceChanged = m_config.trackPreference != config.trackPreference;
    const int fadeDurationMs
        = std::clamp(config.fadeDurationMs, PixmapFadeController::MinDurationMs, PixmapFadeController::MaxDurationMs);
    const int spinSpeedRpm       = std::clamp(config.spinSpeedRpm, 0, MaxSpinSpeedRpm);
    const int spinRampDurationMs = std::clamp(config.spinRampDurationMs, 0, MaxSpinRampDurationMs);

    if(m_config.spinSpeedRpm != spinSpeedRpm || m_config.spinRampDurationMs != spinRampDurationMs) {
        m_discAngle         = discAngle();
        m_spinStartSpeedRpm = discSpeedRpm();
        if(m_rotationTimer.isActive()) {
            m_rotationElapsed.start();
        }
    }

    m_config = {
        .coverType          = config.coverType,
        .coverAlignment     = config.coverAlignment,
        .artworkScaling     = artworkScaling,
        .style              = style,
        .spinDisc           = config.spinDisc,
        .spinSpeedRpm       = spinSpeedRpm,
        .spinRampDurationMs = spinRampDurationMs,
        .trackPreference    = config.trackPreference,
        .fadeCoverChanges   = config.fadeCoverChanges,
        .fadeDurationMs     = fadeDurationMs,
        .doubleClickAction  = config.doubleClickAction,
        .middleClickAction  = config.middleClickAction,
    };

    m_coverType        = m_config.coverType;
    m_coverAlignment   = m_config.coverAlignment;
    m_artworkScaling   = m_config.artworkScaling;
    m_fadeCoverChanges = m_config.fadeCoverChanges;

    m_fadeController->setDurationMs(m_config.fadeDurationMs);
    m_fadeController->setEnabled(m_config.fadeCoverChanges);
    updateDiscRotation();

    Q_EMIT configChanged();

    if(!m_fadeCoverChanges) {
        stopCoverFade();
    }

    if(coverTypeChanged || trackPreferenceChanged) {
        ++m_actionRequestId;
        if(coverTypeChanged) {
            m_noCover = m_coverProvider->placeholderCover(m_coverType);
        }
        reloadCover();
        if(styleChanged) {
            rescaleCover();
        }
        return;
    }

    if(styleChanged || scalingChanged || (alignmentChanged && m_artworkScaling == ArtworkScaling::Fill)) {
        rescaleCover();
        return;
    }

    if(alignmentChanged || fadeEnabledChanged) {
        update();
    }
}

void CoverWidget::saveDefaults(const ConfigData& config) const
{
    m_settings->fileSet(CoverWidgetCoverTypeKey, static_cast<int>(config.coverType));
    m_settings->fileSet(CoverWidgetCoverAlignmentKey, static_cast<int>(config.coverAlignment));
    m_settings->fileSet(CoverWidgetArtworkScalingKey, static_cast<int>(config.artworkScaling));
    m_settings->fileSet(CoverWidgetStyleKey, static_cast<int>(config.style));
    m_settings->fileSet(CoverWidgetSpinDiscKey, config.spinDisc);
    m_settings->fileSet(CoverWidgetSpinSpeedKey, std::clamp(config.spinSpeedRpm, 0, MaxSpinSpeedRpm));
    m_settings->fileSet(CoverWidgetSpinRampDurationKey,
                        std::clamp(config.spinRampDurationMs, 0, MaxSpinRampDurationMs));
    m_settings->fileRemove(CoverWidgetKeepAspectRatioKey);
    m_settings->fileSet(CoverWidgetTrackPreferenceKey, static_cast<int>(config.trackPreference));
    m_settings->fileRemove(CoverWidgetShowStoppedTrackKey);
    m_settings->fileSet(CoverWidgetFadeEnabledKey, config.fadeCoverChanges);
    m_settings->fileSet(
        CoverWidgetFadeDurationKey,
        std::clamp(config.fadeDurationMs, PixmapFadeController::MinDurationMs, PixmapFadeController::MaxDurationMs));
    m_settings->fileSet(CoverWidgetDoubleClickKey, static_cast<int>(config.doubleClickAction));
    m_settings->fileSet(CoverWidgetMiddleClickKey, static_cast<int>(config.middleClickAction));
}

void CoverWidget::clearSavedDefaults() const
{
    m_settings->fileRemove(CoverWidgetCoverTypeKey);
    m_settings->fileRemove(CoverWidgetCoverAlignmentKey);
    m_settings->fileRemove(CoverWidgetKeepAspectRatioKey);
    m_settings->fileRemove(CoverWidgetArtworkScalingKey);
    m_settings->fileRemove(CoverWidgetStyleKey);
    m_settings->fileRemove(CoverWidgetSpinDiscKey);
    m_settings->fileRemove(CoverWidgetSpinSpeedKey);
    m_settings->fileRemove(CoverWidgetSpinRampDurationKey);
    m_settings->fileRemove(CoverWidgetTrackPreferenceKey);
    m_settings->fileRemove(CoverWidgetShowStoppedTrackKey);
    m_settings->fileRemove(CoverWidgetFadeEnabledKey);
    m_settings->fileRemove(CoverWidgetFadeDurationKey);
    m_settings->fileRemove(CoverWidgetDoubleClickKey);
    m_settings->fileRemove(CoverWidgetMiddleClickKey);
}

CoverWidget::ConfigData CoverWidget::configFromLayout(const QJsonObject& layout) const
{
    ConfigData config{defaultConfig()};

    config.coverType
        = static_cast<Track::Cover>(layout.value("CoverType"_L1).toInt(static_cast<int>(config.coverType)));
    config.coverAlignment = static_cast<Qt::Alignment>(layout.value("CoverAlignment"_L1).toInt(config.coverAlignment));
    config.style = layout.value("Style"_L1).toInt(static_cast<int>(config.style)) == static_cast<int>(CoverStyle::Disc)
                     ? CoverStyle::Disc
                     : CoverStyle::Cover;

    if(layout.contains("ArtworkScaling"_L1)) {
        config.artworkScaling = artworkScalingFromInt(layout.value("ArtworkScaling"_L1).toInt());
    }
    else if(layout.contains("KeepAspectRatio"_L1)) {
        config.artworkScaling
            = layout.value("KeepAspectRatio"_L1).toBool(true) ? ArtworkScaling::Fit : ArtworkScaling::Stretch;
    }

    if(layout.contains("TrackPreference"_L1)) {
        config.trackPreference = static_cast<TrackDisplayPreference>(layout.value("TrackPreference"_L1).toInt());
    }
    else if(layout.contains("ShowStoppedTrack"_L1) && !layout.value("ShowStoppedTrack"_L1).toBool()
            && config.trackPreference == TrackDisplayPreference::PlayingTrack) {
        config.trackPreference = TrackDisplayPreference::PlayingTrackBlankWhenStopped;
    }

    config.fadeCoverChanges = layout.value("FadeCoverChanges"_L1).toBool(config.fadeCoverChanges);
    config.spinDisc         = layout.value("SpinDisc"_L1).toBool(config.spinDisc);
    config.spinSpeedRpm = std::clamp(layout.value("SpinSpeedRpm"_L1).toInt(config.spinSpeedRpm), 0, MaxSpinSpeedRpm);
    config.spinRampDurationMs
        = std::clamp(layout.value("SpinRampDurationMs"_L1).toInt(config.spinRampDurationMs), 0, MaxSpinRampDurationMs);
    config.fadeDurationMs    = layout.value("FadeDurationMs"_L1).toInt(config.fadeDurationMs);
    config.doubleClickAction = static_cast<CoverAction>(
        layout.value("DoubleClickAction"_L1).toInt(static_cast<int>(config.doubleClickAction)));
    config.middleClickAction = static_cast<CoverAction>(
        layout.value("MiddleClickAction"_L1).toInt(static_cast<int>(config.middleClickAction)));

    config.fadeDurationMs
        = std::clamp(config.fadeDurationMs, PixmapFadeController::MinDurationMs, PixmapFadeController::MaxDurationMs);

    return config;
}

void CoverWidget::saveConfigToLayout(const ConfigData& config, QJsonObject& layout)
{
    layout["CoverType"_L1]          = static_cast<int>(config.coverType);
    layout["CoverAlignment"_L1]     = static_cast<int>(config.coverAlignment);
    layout["ArtworkScaling"_L1]     = static_cast<int>(config.artworkScaling);
    layout["Style"_L1]              = static_cast<int>(config.style);
    layout["SpinDisc"_L1]           = config.spinDisc;
    layout["SpinSpeedRpm"_L1]       = std::clamp(config.spinSpeedRpm, 0, MaxSpinSpeedRpm);
    layout["SpinRampDurationMs"_L1] = std::clamp(config.spinRampDurationMs, 0, MaxSpinRampDurationMs);
    layout.remove("KeepAspectRatio"_L1);
    layout["TrackPreference"_L1] = static_cast<int>(config.trackPreference);
    layout.remove("ShowStoppedTrack"_L1);
    layout["FadeCoverChanges"_L1]  = config.fadeCoverChanges;
    layout["FadeDurationMs"_L1]    = config.fadeDurationMs;
    layout["DoubleClickAction"_L1] = static_cast<int>(config.doubleClickAction);
    layout["MiddleClickAction"_L1] = static_cast<int>(config.middleClickAction);
}

void CoverWidget::rescaleCover()
{
    m_scaledCover = scaledCover(m_fadeController->pixmap());

    if(!m_fadeController->previousPixmap().isNull()) {
        m_previousScaledCover = scaledCover(m_fadeController->previousPixmap());
    }
    else {
        m_previousScaledCover = {};
    }

    update();
}

void CoverWidget::reloadCover()
{
    const int requestId = ++m_coverRequestId;
    m_track             = displayTrack();
    updateDiscRotation();

    if(!m_track.isValid()) {
        setCoverPixmap({});
        return;
    }

    if(m_cover.isNull()) {
        setCoverPixmap({});
    }

    m_coverProvider->trackCoverFull(m_track, m_coverType).then(this, [this, requestId](const QPixmap& cover) {
        if(requestId != m_coverRequestId) {
            return;
        }

        setCoverPixmap(cover);
    });
}

QString CoverWidget::name() const
{
    return tr("Artwork Panel");
}

QString CoverWidget::layoutName() const
{
    return u"ArtworkPanel"_s;
}

void CoverWidget::saveLayoutData(QJsonObject& layout)
{
    saveConfigToLayout(m_config, layout);
}

void CoverWidget::loadLayoutData(const QJsonObject& layout)
{
    applyConfig(configFromLayout(layout));
}

void CoverWidget::resizeEvent(QResizeEvent* event)
{
    m_resizeTimer.start(ResizeInterval, this);

    QWidget::resizeEvent(event);
}

void CoverWidget::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* scalingMenu = new QMenu(tr("Artwork scaling"), menu);
    scalingMenu->setEnabled(m_config.style == CoverStyle::Cover);
    auto* scalingGroup = new QActionGroup(scalingMenu);

    const auto addScalingAction = [this, scalingMenu, scalingGroup](const QString& text, ArtworkScaling scaling) {
        auto* action = scalingMenu->addAction(text);
        scalingGroup->addAction(action);
        action->setCheckable(true);
        action->setChecked(m_config.artworkScaling == scaling);
        QObject::connect(action, &QAction::triggered, this, [this, scaling]() {
            auto config{m_config};
            config.artworkScaling = scaling;
            applyConfig(config);
        });
    };
    addScalingAction(tr("Fit"), ArtworkScaling::Fit);
    addScalingAction(tr("Fill (crop)"), ArtworkScaling::Fill);
    addScalingAction(tr("Stretch"), ArtworkScaling::Stretch);

    auto* styleMenu  = new QMenu(tr("Style"), menu);
    auto* styleGroup = new QActionGroup(styleMenu);
    for(const auto& [text, style] :
        {std::pair{tr("Cover"), CoverStyle::Cover}, std::pair{tr("Disc"), CoverStyle::Disc}}) {
        auto* action = styleMenu->addAction(text);
        styleGroup->addAction(action);
        action->setCheckable(true);
        action->setChecked(m_config.style == style);
        QObject::connect(action, &QAction::triggered, this, [this, style]() {
            auto config{m_config};
            config.style = style;
            applyConfig(config);
        });
    }

    auto* alignmentMenu  = new QMenu(tr("Alignment"), menu);
    auto* alignmentGroup = new QActionGroup(alignmentMenu);

    auto* alignCenter = new QAction(tr("Centre"), alignmentGroup);
    auto* alignLeft   = new QAction(tr("Left"), alignmentGroup);
    auto* alignRight  = new QAction(tr("Right"), alignmentGroup);

    alignCenter->setCheckable(true);
    alignLeft->setCheckable(true);
    alignRight->setCheckable(true);

    alignCenter->setChecked(m_config.coverAlignment == Qt::AlignCenter);
    alignLeft->setChecked(m_config.coverAlignment == Qt::AlignLeft);
    alignRight->setChecked(m_config.coverAlignment == Qt::AlignRight);

    const auto changeAlignment = [this](Qt::Alignment alignment) {
        auto config{m_config};
        config.coverAlignment = alignment;
        applyConfig(config);
    };

    QObject::connect(alignCenter, &QAction::triggered, this, [changeAlignment]() { changeAlignment(Qt::AlignCenter); });
    QObject::connect(alignLeft, &QAction::triggered, this, [changeAlignment]() { changeAlignment(Qt::AlignLeft); });
    QObject::connect(alignRight, &QAction::triggered, this, [changeAlignment]() { changeAlignment(Qt::AlignRight); });

    alignmentMenu->addAction(alignCenter);
    alignmentMenu->addAction(alignLeft);
    alignmentMenu->addAction(alignRight);

    auto* coverGroup = new QActionGroup(menu);

    auto* frontCover  = new QAction(tr("Front cover"), coverGroup);
    auto* backCover   = new QAction(tr("Back cover"), coverGroup);
    auto* artistCover = new QAction(tr("Artist"), coverGroup);

    frontCover->setCheckable(true);
    backCover->setCheckable(true);
    artistCover->setCheckable(true);

    frontCover->setChecked(m_config.coverType == Track::Cover::Front);
    backCover->setChecked(m_config.coverType == Track::Cover::Back);
    artistCover->setChecked(m_config.coverType == Track::Cover::Artist);

    auto reload = [this](Track::Cover type) {
        auto config{m_config};
        config.coverType = type;
        applyConfig(config);
    };

    QObject::connect(frontCover, &QAction::triggered, this, [reload]() { reload(Track::Cover::Front); });
    QObject::connect(backCover, &QAction::triggered, this, [reload]() { reload(Track::Cover::Back); });
    QObject::connect(artistCover, &QAction::triggered, this, [reload]() { reload(Track::Cover::Artist); });

    menu->addAction(frontCover);
    menu->addAction(backCover);
    menu->addAction(artistCover);
    menu->addSeparator();
    menu->addMenu(styleMenu);
    menu->addMenu(scalingMenu);
    menu->addMenu(alignmentMenu);
    menu->addSeparator();

    auto* artworkSettings = new QAction(tr("Artwork settings…"), menu);
    QObject::connect(artworkSettings, &QAction::triggered, this, [this]() {
        if(m_settings && m_settings->settingsDialog()) {
            m_settings->settingsDialog()->openAtPage(Constants::Page::ArtworkGeneral);
        }
    });
    menu->addAction(artworkSettings);

    addConfigureAction(menu, false);

    if(m_track.isValid()) {
        auto* viewFullSize  = new QAction(tr("View full size"), menu);
        const auto canWrite = [this]() {
            return !m_track.hasCue() && !m_track.isInArchive() && m_audioLoader->canWriteMetadata(m_track);
        };
        const auto canWriteCover = [this, canWrite]() {
            return canWrite()
                || m_settings->value<Settings::Gui::Internal::ArtworkSaveMethods>()
                           .value<ArtworkSaveMethods>()
                           .value(m_config.coverType)
                           .method
                       == ArtworkSaveMethod::Directory;
        };

        auto* search      = new QAction(tr("Search for artwork…"), menu);
        auto* quickSearch = new QAction(tr("Quicksearch for artwork"), menu);
        auto* extractFile = new QAction(tr("Auto-extract to file"), menu);
        auto* extractAs   = new QAction(tr("Extract as…"), menu);
        auto* remove      = new QAction(tr("Remove artwork"), menu);

        for(const auto& action : {search, quickSearch, remove}) {
            action->setEnabled(canWriteCover());
        }
        search->setStatusTip(tr("Search for artwork for this cover type"));
        quickSearch->setStatusTip(tr("Search for artwork and automatically choose the best match for this cover type"));
        extractFile->setStatusTip(
            tr("Extract this embedded artwork to a file in the track directory without prompting"));
        extractAs->setStatusTip(tr("Choose where to extract this embedded artwork"));
        remove->setStatusTip(tr("Remove this artwork"));

        extractFile->setEnabled(m_track.isValid());
        extractAs->setEnabled(m_track.isValid());

        if(m_config.coverType == Track::Cover::Back) {
            // Only support front and artist cover for now
            search->setDisabled(true);
            quickSearch->setDisabled(true);
            remove->setDisabled(true);
        }

        if(m_cover.isNull()) {
            extractFile->setDisabled(true);
            extractAs->setDisabled(true);
        }
        if(m_cover.isNull()) {
            viewFullSize->setDisabled(true);
            remove->setDisabled(true);
        }

        QObject::connect(viewFullSize, &QAction::triggered, this, &CoverWidget::showArtworkViewer);
        QObject::connect(search, &QAction::triggered, this,
                         [this]() { Q_EMIT requestArtworkSearch({m_track}, m_config.coverType, false); });
        QObject::connect(quickSearch, &QAction::triggered, this,
                         [this]() { Q_EMIT requestArtworkSearch({m_track}, m_config.coverType, true); });
        QObject::connect(extractFile, &QAction::triggered, this, [this]() {
            const auto summary = ArtworkExporter::extractTracks(m_audioLoader.get(), {m_track}, {m_config.coverType});
            StatusEvent::post(ArtworkExporter::statusMessage(summary));
        });
        QObject::connect(extractAs, &QAction::triggered, this, [this]() {
            if(const QString path
               = ArtworkExporter::extractTrackAs(m_audioLoader.get(), m_track, m_config.coverType, this);
               !path.isEmpty()) {
                StatusEvent::post(tr("Extracted artwork to %1").arg(QDir::toNativeSeparators(path)));
            }
        });
        QObject::connect(remove, &QAction::triggered, this,
                         [this]() { Q_EMIT requestArtworkRemoval({m_track}, m_config.coverType); });

        menu->addSeparator();
        menu->addAction(viewFullSize);
        menu->addSeparator();
        menu->addAction(search);
        menu->addAction(quickSearch);
        menu->addSeparator();
        menu->addAction(extractFile);
        menu->addAction(extractAs);
        menu->addSeparator();
        menu->addAction(remove);
    }

    menu->popup(event->globalPos());
}

void CoverWidget::mousePressEvent(QMouseEvent* event)
{
    if(event->button() == Qt::MiddleButton) {
        performAction(m_config.middleClickAction);
        event->accept();
        return;
    }

    FyWidget::mousePressEvent(event);
}

void CoverWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    if(event->button() == Qt::LeftButton) {
        performAction(m_config.doubleClickAction);
        event->accept();
        return;
    }

    FyWidget::mouseDoubleClickEvent(event);
}

void CoverWidget::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_resizeTimer.timerId()) {
        m_resizeTimer.stop();
        rescaleCover();
        return;
    }
    if(event->timerId() == m_rotationTimer.timerId()) {
        if(m_spinTargetSpeedRpm == 0.0 && discSpeedRpm() == 0.0) {
            m_discAngle = discAngle();
            m_rotationTimer.stop();
        }
        update();
        return;
    }
    FyWidget::timerEvent(event);
}

void CoverWidget::paintEvent(QPaintEvent* /*event*/)
{
    QStylePainter painter{this};

    if(m_config.style == CoverStyle::Disc) {
        const auto discRect = QStyle::alignedRect(layoutDirection(), Qt::AlignVCenter | m_coverAlignment,
                                                  m_scaledCover.deviceIndependentSize().toSize(), contentsRect());
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.translate(QRectF{discRect}.center());
        painter.rotate(discAngle());
        painter.translate(-QRectF{discRect}.center());
    }

    if(m_fadeController->progress() < 1.0) {
        if(!m_previousScaledCover.isNull()) {
            painter.setOpacity(1.0 - m_fadeController->progress());
            painter.drawItemPixmap(contentsRect(), Qt::AlignVCenter | m_coverAlignment, m_previousScaledCover);
        }
        painter.setOpacity(m_fadeController->progress());
    }

    painter.drawItemPixmap(contentsRect(), Qt::AlignVCenter | m_coverAlignment, m_scaledCover);
}

void CoverWidget::openConfigDialog()
{
    showConfigDialog(new CoverWidgetConfigDialog(this, this), Qt::NonModal);
}

bool CoverWidget::coversMatch(const QPixmap& lhs, const QPixmap& rhs) const
{
    const QPixmap effectiveLhs = effectiveCover(lhs);
    const QPixmap effectiveRhs = effectiveCover(rhs);

    return PixmapFadeController::pixmapsMatch(effectiveLhs, effectiveRhs);
}

QPixmap CoverWidget::effectiveCover(const QPixmap& cover) const
{
    return cover.isNull() ? m_noCover : cover;
}

QPixmap CoverWidget::scaledCover(const QPixmap& cover) const
{
    if(m_config.style == CoverStyle::Disc) {
        return scaledDiscCover(cover);
    }

    return Gui::scaleArtworkPixmap(effectiveCover(cover), contentsRect().size(), devicePixelRatioF(), m_artworkScaling,
                                   Qt::AlignVCenter | m_coverAlignment);
}

QPixmap CoverWidget::scaledDiscCover(const QPixmap& cover) const
{
    const int diameter = std::min(contentsRect().width(), contentsRect().height());
    if(diameter <= 2) {
        return {};
    }

    const QPixmap artwork = Gui::scaleArtworkPixmap(effectiveCover(cover), QSize{diameter, diameter},
                                                    devicePixelRatioF(), ArtworkScaling::Fill, Qt::AlignCenter);
    if(artwork.isNull()) {
        return {};
    }

    QPixmap disc{artwork.size()};
    disc.setDevicePixelRatio(artwork.devicePixelRatio());
    disc.fill(Qt::transparent);

    const QRectF outer     = QRectF{QPointF{}, disc.deviceIndependentSize()}.adjusted(1.0, 1.0, -1.0, -1.0);
    const QPointF centre   = outer.center();
    const qreal radius     = outer.width() / 2.0;
    const qreal holeRadius = radius * 0.132;
    const qreal hubRadius  = radius * 0.28;

    QPainterPath outline;
    outline.addEllipse(outer);
    outline.addEllipse(centre, holeRadius, holeRadius);

    QPainter painter{&disc};
    painter.setRenderHint(QPainter::Antialiasing);
    painter.drawPixmap(0, 0, artwork);

    QConicalGradient sheen{centre, 30.0};
    sheen.setColorAt(0.0, QColor{255, 110, 90, 35});
    sheen.setColorAt(0.16, QColor{255, 235, 120, 45});
    sheen.setColorAt(0.32, QColor{100, 255, 185, 35});
    sheen.setColorAt(0.48, QColor{235, 250, 255, 65});
    sheen.setColorAt(0.58, QColor{110, 170, 255, 40});
    sheen.setColorAt(0.74, QColor{230, 130, 255, 45});
    sheen.setColorAt(0.9, QColor{255, 225, 245, 65});
    sheen.setColorAt(1.0, QColor{255, 110, 90, 35});
    painter.fillRect(disc.rect(), sheen);

    QRadialGradient rim{centre, radius};
    rim.setColorAt(0.0, Qt::transparent);
    rim.setColorAt(0.965, Qt::transparent);
    rim.setColorAt(0.98, QColor{225, 230, 240, 110});
    rim.setColorAt(0.99, QColor{255, 255, 255, 210});
    rim.setColorAt(1.0, QColor{65, 70, 80, 160});
    painter.fillRect(disc.rect(), rim);

    QRadialGradient hub{centre, hubRadius};
    hub.setColorAt(0.0, QColor{225, 228, 235, 230});
    hub.setColorAt(0.48, QColor{250, 251, 255, 245});
    hub.setColorAt(0.52, QColor{180, 185, 195, 210});
    hub.setColorAt(0.78, QColor{225, 228, 235, 220});
    hub.setColorAt(0.8, QColor{250, 251, 255, 240});
    hub.setColorAt(0.94, QColor{210, 215, 225, 220});
    hub.setColorAt(0.97, QColor{65, 70, 80, 220});
    hub.setColorAt(1.0, QColor{240, 242, 248, 220});
    painter.setPen(Qt::NoPen);
    painter.setBrush(hub);
    painter.drawEllipse(centre, hubRadius, hubRadius);

    QPixmap mask{disc.size()};
    mask.setDevicePixelRatio(disc.devicePixelRatio());
    mask.fill(Qt::transparent);
    QPainter maskPainter{&mask};
    maskPainter.setRenderHint(QPainter::Antialiasing);
    maskPainter.fillPath(outline, Qt::white);
    maskPainter.end();
    painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    painter.drawPixmap(0, 0, mask);

    return disc;
}

qreal CoverWidget::discAngle() const
{
    const qreal elapsed = m_rotationTimer.isActive() ? m_rotationElapsed.elapsed() : 0;
    qreal rotation      = elapsed * m_spinTargetSpeedRpm;
    if(m_config.spinRampDurationMs > 0) {
        const qreal rampElapsed = std::min(elapsed, static_cast<qreal>(m_config.spinRampDurationMs));
        rotation -= (m_spinTargetSpeedRpm - m_spinStartSpeedRpm) * rampElapsed
                  * (1.0 - (rampElapsed / (2.0 * m_config.spinRampDurationMs)));
    }
    return std::fmod(m_discAngle + (rotation * 360.0 / 60000.0), 360.0);
}

qreal CoverWidget::discSpeedRpm() const
{
    if(!m_rotationTimer.isActive()) {
        return 0.0;
    }
    if(m_config.spinRampDurationMs == 0) {
        return m_spinTargetSpeedRpm;
    }

    const qreal progress = std::min(1.0, static_cast<qreal>(m_rotationElapsed.elapsed()) / m_config.spinRampDurationMs);
    return std::lerp(m_spinStartSpeedRpm, m_spinTargetSpeedRpm, progress);
}

void CoverWidget::updateDiscRotation()
{
    const bool canRotate = m_config.style == CoverStyle::Disc && !m_cover.isNull() && m_track.isValid()
                        && sameDisplayTrack(m_track, m_playerController->currentTrack());
    if(!canRotate) {
        if(m_rotationTimer.isActive()) {
            m_discAngle = discAngle();
            m_rotationTimer.stop();
            update();
        }
        m_spinStartSpeedRpm  = 0.0;
        m_spinTargetSpeedRpm = 0.0;
        return;
    }

    const qreal targetSpeed
        = m_playerController->playState() == Player::PlayState::Playing ? m_config.spinSpeedRpm : 0.0;
    if(targetSpeed == m_spinTargetSpeedRpm) {
        return;
    }

    m_discAngle          = discAngle();
    m_spinStartSpeedRpm  = discSpeedRpm();
    m_spinTargetSpeedRpm = targetSpeed;
    m_rotationElapsed.start();

    if(targetSpeed > 0.0 || (m_spinStartSpeedRpm > 0.0 && m_config.spinRampDurationMs > 0)) {
        if(!m_rotationTimer.isActive()) {
            m_rotationTimer.start(RotationInterval, Qt::PreciseTimer, this);
        }
    }
    else {
        m_rotationTimer.stop();
    }

    update();
}

Track CoverWidget::displayTrack() const
{
    const auto source = preferredTrackSource(m_config.trackPreference, m_playerController->playState(),
                                             m_playerController->playbackStarted());
    if(source == PreferredTrackSource::None) {
        return {};
    }

    const auto playingTrack = [this]() {
        if(const Track track = m_playerController->currentTrack(); track.isValid()) {
            return track;
        }
        if(const PlaylistTrack track = m_playlistHandler->currentTrack(); track.isValid()) {
            return track.track;
        }
        return Track{};
    };

    const auto selectedTrack = [this]() {
        return m_trackSelection->hasDisplayTracks() ? m_trackSelection->displayTrack() : Track{};
    };

    if(source == PreferredTrackSource::Selected) {
        if(const Track track = selectedTrack(); track.isValid()) {
            return track;
        }
        return playingTrack();
    }

    if(const Track track = playingTrack(); track.isValid()) {
        return track;
    }

    return selectedTrack();
}

bool CoverWidget::sameDisplayTrack(const Track& lhs, const Track& rhs)
{
    if(!lhs.isValid() || !rhs.isValid()) {
        return lhs.isValid() == rhs.isValid();
    }

    return lhs.filepath() == rhs.filepath() && lhs.subsong() == rhs.subsong() && lhs.offset() == rhs.offset();
}

void CoverWidget::setFadeCoverChanges(const bool enabled)
{
    m_fadeCoverChanges = enabled;
    m_fadeController->setEnabled(enabled);
}

void CoverWidget::stopCoverFade()
{
    m_fadeController->stop();
}

void CoverWidget::setCoverPixmap(const QPixmap& cover)
{
    if(coversMatch(m_cover, cover)) {
        m_cover = cover;
        updateDiscRotation();
        if(m_fadeController->progress() < 1.0
           && PixmapFadeController::pixmapsMatch(m_fadeController->pixmap(), effectiveCover(cover))) {
            return;
        }
        m_fadeController->setPixmap(effectiveCover(cover));
        rescaleCover();
        return;
    }

    m_cover = cover;
    updateDiscRotation();
    m_fadeController->setPixmap(effectiveCover(cover));
    rescaleCover();
}

void CoverWidget::handleSelectionChanged()
{
    if(sameDisplayTrack(displayTrack(), m_track)) {
        return;
    }

    reloadCover();
}

void CoverWidget::performAction(const CoverAction action)
{
    ++m_actionRequestId;

    switch(action) {
        case CoverAction::None:
            break;
        case CoverAction::ViewFullSize:
            showArtworkViewer();
            break;
        case CoverAction::OpenContainingFolder:
            openContainingFolder();
            break;
        case CoverAction::NextArtworkType:
            showNextArtworkType();
            break;
        case CoverAction::ShowTrack:
            showTrack();
            break;
        case CoverAction::OpenProperties:
            if(m_track.isValid()) {
                Q_EMIT requestPropertiesDialog({m_track});
            }
            break;
    }
}

void CoverWidget::showArtworkViewer()
{
    if(!m_track.isValid() || m_cover.isNull()) {
        return;
    }

    auto* dialog = new ArtworkViewerDialog(m_track, m_cover, this);
    dialog->show();

    m_coverProvider->trackCoverOriginal(m_track, m_config.coverType).then(dialog, [dialog](const QPixmap& cover) {
        if(!cover.isNull()) {
            dialog->setCover(cover);
        }
    });
}

void CoverWidget::openContainingFolder() const
{
    if(!m_track.isValid()) {
        return;
    }

    const QString dir = m_track.isInArchive() ? QFileInfo{m_track.archivePath()}.absolutePath() : m_track.path();
    if(!dir.isEmpty()) {
        Utils::File::openDirectory(dir);
    }
}

void CoverWidget::showTrack() const
{
    if(auto* command = m_actionManager->command(Constants::Actions::ShowNowPlaying)) {
        command->action()->trigger();
    }
}

void CoverWidget::showNextArtworkType()
{
    if(!m_track.isValid()) {
        return;
    }

    Track::Cover nextType{Track::Cover::Front};
    switch(m_config.coverType) {
        case Track::Cover::Front:
            nextType = Track::Cover::Back;
            break;
        case Track::Cover::Back:
            nextType = Track::Cover::Artist;
            break;
        case Track::Cover::Artist:
        case Track::Cover::Other:
            break;
    }

    tryArtworkType(m_track, nextType, 2, m_actionRequestId);
}

void CoverWidget::tryArtworkType(const Track& track, const Track::Cover type, const int remaining, const int requestId)
{
    m_coverProvider->trackHasCover(track, type).then(this, [this, track, type, remaining, requestId](bool found) {
        if(requestId != m_actionRequestId || !sameDisplayTrack(track, m_track)) {
            return;
        }

        if(found) {
            auto config{m_config};
            config.coverType = type;
            applyConfig(config);
            return;
        }

        if(remaining > 1) {
            const Track::Cover nextType = type == Track::Cover::Front ? Track::Cover::Back
                                        : type == Track::Cover::Back  ? Track::Cover::Artist
                                                                      : Track::Cover::Front;
            tryArtworkType(track, nextType, remaining - 1, requestId);
        }
    });
}

void CoverWidget::checkTrackArtwork(const Track& track)
{
    if(m_settings->value<Settings::Gui::Internal::ArtworkAutoSearch>()) {
        if(track.isValid()) {
            m_coverProvider->trackHasCover(track, m_config.coverType).then([this](const bool hasCover) {
                if(!hasCover) {
                    Q_EMIT requestArtworkSearch({m_track}, m_config.coverType, true);
                }
            });
        }
    }
}
} // namespace Fooyin

#include "moc_coverwidget.cpp"
