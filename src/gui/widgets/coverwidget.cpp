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

#include "coverwidget.h"

#include "artwork/artworkexporter.h"
#include "artwork/artworkviewerdialog.h"
#include "coverwidgetconfigwidget.h"
#include "statusevent.h"

#include <core/engine/audioloader.h>
#include <core/player/playercontroller.h>
#include <core/track.h>
#include <gui/coverprovider.h>
#include <gui/guisettings.h>
#include <gui/trackselectioncontroller.h>
#include <utils/settings/settingsmanager.h>

#include <QAction>
#include <QActionGroup>
#include <QContextMenuEvent>
#include <QDir>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QMenu>
#include <QMouseEvent>
#include <QStylePainter>
#include <QTimer>
#include <QTimerEvent>
#include <QVariantAnimation>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;

constexpr int MinCoverFadeDurationMs = 50;
constexpr int MaxCoverFadeDurationMs = 3000;

constexpr auto CoverWidgetCoverTypeKey       = u"ArtworkPanel/CoverType";
constexpr auto CoverWidgetCoverAlignmentKey  = u"ArtworkPanel/CoverAlignment";
constexpr auto CoverWidgetKeepAspectRatioKey = u"ArtworkPanel/KeepAspectRatio";
constexpr auto CoverWidgetFadeEnabledKey     = u"ArtworkPanel/FadeCoverChanges";
constexpr auto CoverWidgetFadeDurationKey    = u"ArtworkPanel/FadeDurationMs";

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
constexpr auto ResizeInterval = 5ms;
#else
constexpr auto ResizeInterval = 5;
#endif

namespace Fooyin {
CoverWidget::CoverWidget(PlayerController* playerController, TrackSelectionController* trackSelection,
                         std::shared_ptr<AudioLoader> audioLoader, SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_playerController{playerController}
    , m_trackSelection{trackSelection}
    , m_audioLoader{audioLoader}
    , m_settings{settings}
    , m_coverProvider{new CoverProvider(audioLoader, settings, this)}
    , m_displayOption{static_cast<SelectionDisplay>(
          m_settings->value<Settings::Gui::Internal::TrackCoverDisplayOption>())}
    , m_fadeAnimation{new QVariantAnimation(this)}
    , m_coverRequestId{0}
    , m_fadeProgress{1.0}
    , m_noCover{m_coverProvider->placeholderCover()}
{
    setObjectName(CoverWidget::name());

    m_coverProvider->setUsePlaceholder(false);

    m_fadeAnimation->setStartValue(0.0);
    m_fadeAnimation->setEndValue(1.0);
    m_fadeAnimation->setEasingCurve(QEasingCurve::InOutCubic);

    QObject::connect(m_fadeAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        m_fadeProgress = value.toReal();
        update();
    });
    QObject::connect(m_fadeAnimation, &QVariantAnimation::finished, this, [this] {
        m_previousCover       = {};
        m_previousScaledCover = {};
        m_fadeProgress        = 1.0;
        update();
    });

    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this, &CoverWidget::reloadCover);
    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this, &CoverWidget::checkTrackArtwork);
    QObject::connect(m_trackSelection, &TrackSelectionController::selectionChanged, this,
                     &CoverWidget::handleSelectionChanged);
    QObject::connect(m_coverProvider, &CoverProvider::coverAdded, this, &CoverWidget::reloadCover,
                     Qt::QueuedConnection);

    m_settings->subscribe<Settings::Gui::Internal::TrackCoverDisplayOption>(this, [this](const int option) {
        m_displayOption = static_cast<SelectionDisplay>(option);
        reloadCover();
    });
    m_settings->subscribe<Settings::Gui::IconTheme>(this, &CoverWidget::reloadCover);
    m_settings->subscribe<Settings::Gui::Theme>(this, &CoverWidget::reloadCover);
    m_settings->subscribe<Settings::Gui::Style>(this, &CoverWidget::reloadCover);

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
    config.keepAspectRatio  = m_settings->fileValue(CoverWidgetKeepAspectRatioKey, config.keepAspectRatio).toBool();
    config.fadeCoverChanges = m_settings->fileValue(CoverWidgetFadeEnabledKey, config.fadeCoverChanges).toBool();
    config.fadeDurationMs   = m_settings->fileValue(CoverWidgetFadeDurationKey, config.fadeDurationMs).toInt();
    config.fadeDurationMs   = std::clamp(config.fadeDurationMs, MinCoverFadeDurationMs, MaxCoverFadeDurationMs);

    return config;
}

const CoverWidget::ConfigData& CoverWidget::currentConfig() const
{
    return m_config;
}

void CoverWidget::applyConfig(const ConfigData& config)
{
    const bool coverTypeChanged   = m_coverType != config.coverType;
    const bool keepAspectChanged  = m_keepAspectRatio != config.keepAspectRatio;
    const bool alignmentChanged   = m_coverAlignment != config.coverAlignment;
    const bool fadeEnabledChanged = m_fadeCoverChanges != config.fadeCoverChanges;
    const int fadeDurationMs      = std::clamp(config.fadeDurationMs, MinCoverFadeDurationMs, MaxCoverFadeDurationMs);

    m_config = {
        .coverType        = config.coverType,
        .coverAlignment   = config.coverAlignment,
        .keepAspectRatio  = config.keepAspectRatio,
        .fadeCoverChanges = config.fadeCoverChanges,
        .fadeDurationMs   = fadeDurationMs,
    };

    m_coverType        = m_config.coverType;
    m_coverAlignment   = m_config.coverAlignment;
    m_keepAspectRatio  = m_config.keepAspectRatio;
    m_fadeCoverChanges = m_config.fadeCoverChanges;

    m_fadeAnimation->setDuration(m_config.fadeDurationMs);

    if(!m_fadeCoverChanges) {
        stopCoverFade();
    }

    if(coverTypeChanged) {
        reloadCover();
        return;
    }

    if(keepAspectChanged) {
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
    m_settings->fileSet(CoverWidgetKeepAspectRatioKey, config.keepAspectRatio);
    m_settings->fileSet(CoverWidgetFadeEnabledKey, config.fadeCoverChanges);
    m_settings->fileSet(CoverWidgetFadeDurationKey,
                        std::clamp(config.fadeDurationMs, MinCoverFadeDurationMs, MaxCoverFadeDurationMs));
}

void CoverWidget::clearSavedDefaults() const
{
    m_settings->fileRemove(CoverWidgetCoverTypeKey);
    m_settings->fileRemove(CoverWidgetCoverAlignmentKey);
    m_settings->fileRemove(CoverWidgetKeepAspectRatioKey);
    m_settings->fileRemove(CoverWidgetFadeEnabledKey);
    m_settings->fileRemove(CoverWidgetFadeDurationKey);
}

QPixmap CoverWidget::effectiveCover(const QPixmap& cover) const
{
    return cover.isNull() ? m_noCover : cover;
}

bool CoverWidget::coversMatch(const QPixmap& lhs, const QPixmap& rhs) const
{
    const QPixmap effectiveLhs = effectiveCover(lhs);
    const QPixmap effectiveRhs = effectiveCover(rhs);

    if(effectiveLhs.cacheKey() == effectiveRhs.cacheKey()) {
        return true;
    }

    if(effectiveLhs.size() != effectiveRhs.size()
       || !qFuzzyCompare(effectiveLhs.devicePixelRatio(), effectiveRhs.devicePixelRatio())) {
        return false;
    }

    return effectiveLhs.toImage() == effectiveRhs.toImage();
}

QPixmap CoverWidget::scaledCover(const QPixmap& cover) const
{
    const auto aspectRatio = m_keepAspectRatio ? Qt::KeepAspectRatio : Qt::IgnoreAspectRatio;
    const double dpr       = devicePixelRatioF();
    const QSize scaledSize = size() * dpr;

    QPixmap scaled = effectiveCover(cover).scaled(scaledSize, aspectRatio, Qt::SmoothTransformation);
    scaled.setDevicePixelRatio(dpr);

    return scaled;
}

Track CoverWidget::displayTrack() const
{
    if(m_displayOption == SelectionDisplay::PreferSelection && m_trackSelection->hasTracks()) {
        return m_trackSelection->selectedTrack();
    }

    return m_playerController->currentTrack();
}

bool CoverWidget::sameDisplayTrack(const Track& lhs, const Track& rhs)
{
    if(!lhs.isValid() || !rhs.isValid()) {
        return lhs.isValid() == rhs.isValid();
    }

    return lhs.filepath() == rhs.filepath() && lhs.subsong() == rhs.subsong() && lhs.offset() == rhs.offset();
}

void CoverWidget::rescaleCover()
{
    m_scaledCover = scaledCover(m_cover);

    if(!m_previousCover.isNull()) {
        m_previousScaledCover = scaledCover(m_previousCover);
    }
    else {
        m_previousScaledCover = {};
    }

    update();
}

void CoverWidget::setFadeCoverChanges(const bool enabled)
{
    m_fadeCoverChanges = enabled;

    if(!m_fadeCoverChanges) {
        stopCoverFade();
    }
}

void CoverWidget::stopCoverFade()
{
    m_fadeAnimation->stop();
    m_previousCover       = {};
    m_previousScaledCover = {};
    m_fadeProgress        = 1.0;
    update();
}

void CoverWidget::setCoverPixmap(const QPixmap& cover)
{
    const QPixmap currentCover = effectiveCover(m_cover);

    if(coversMatch(m_cover, cover)) {
        m_cover = cover;
        stopCoverFade();
        rescaleCover();
        return;
    }

    m_cover = cover;

    if(!m_fadeCoverChanges || currentCover.isNull()) {
        stopCoverFade();
        rescaleCover();
        return;
    }

    m_previousCover = currentCover;
    m_fadeAnimation->stop();
    m_fadeProgress = 0.0;
    rescaleCover();
    m_fadeAnimation->start();
}

void CoverWidget::reloadCover()
{
    const int requestId = ++m_coverRequestId;
    m_track             = displayTrack();

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

void CoverWidget::handleSelectionChanged()
{
    if(m_displayOption != SelectionDisplay::PreferSelection) {
        return;
    }

    if(sameDisplayTrack(displayTrack(), m_track)) {
        return;
    }

    reloadCover();
}

QString CoverWidget::name() const
{
    return tr("Artwork Panel");
}

QString CoverWidget::layoutName() const
{
    return u"ArtworkPanel"_s;
}

CoverWidget::ConfigData CoverWidget::configFromLayout(const QJsonObject& layout) const
{
    ConfigData config{defaultConfig()};

    if(layout.contains("CoverType"_L1)) {
        config.coverType = static_cast<Track::Cover>(layout.value("CoverType"_L1).toInt());
    }
    if(layout.contains("CoverAlignment"_L1)) {
        config.coverAlignment = static_cast<Qt::Alignment>(layout.value("CoverAlignment"_L1).toInt());
    }
    if(layout.contains("KeepAspectRatio"_L1)) {
        config.keepAspectRatio = layout.value("KeepAspectRatio"_L1).toBool();
    }
    if(layout.contains("FadeCoverChanges"_L1)) {
        config.fadeCoverChanges = layout.value("FadeCoverChanges"_L1).toBool();
    }
    if(layout.contains("FadeDurationMs"_L1)) {
        config.fadeDurationMs = layout.value("FadeDurationMs"_L1).toInt();
    }

    config.fadeDurationMs = std::clamp(config.fadeDurationMs, MinCoverFadeDurationMs, MaxCoverFadeDurationMs);

    return config;
}

void CoverWidget::saveConfigToLayout(const ConfigData& config, QJsonObject& layout)
{
    layout["CoverType"_L1]        = static_cast<int>(config.coverType);
    layout["CoverAlignment"_L1]   = static_cast<int>(config.coverAlignment);
    layout["KeepAspectRatio"_L1]  = config.keepAspectRatio;
    layout["FadeCoverChanges"_L1] = config.fadeCoverChanges;
    layout["FadeDurationMs"_L1]   = config.fadeDurationMs;
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
    if(!m_scaledCover.isNull()) {
        m_resizeTimer.start(ResizeInterval, this);
    }

    QWidget::resizeEvent(event);
}

void CoverWidget::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* keepAspectRatio = new QAction(tr("Keep aspect ratio"), menu);

    keepAspectRatio->setCheckable(true);
    keepAspectRatio->setChecked(m_config.keepAspectRatio);

    QObject::connect(keepAspectRatio, &QAction::triggered, this, [this](bool checked) {
        auto config{m_config};
        config.keepAspectRatio = checked;
        applyConfig(config);
    });

    auto* alignmentGroup = new QActionGroup(menu);

    auto* alignCenter = new QAction(tr("Align to centre"), alignmentGroup);
    auto* alignLeft   = new QAction(tr("Align to left"), alignmentGroup);
    auto* alignRight  = new QAction(tr("Align to right"), alignmentGroup);

    alignCenter->setCheckable(true);
    alignLeft->setCheckable(true);
    alignRight->setCheckable(true);

    alignCenter->setChecked(m_config.coverAlignment == Qt::AlignCenter);
    alignLeft->setChecked(m_config.coverAlignment == Qt::AlignLeft);
    alignRight->setChecked(m_config.coverAlignment == Qt::AlignRight);

    auto changeAlignment = [this](Qt::Alignment alignment) {
        auto config{m_config};
        config.coverAlignment = alignment;
        applyConfig(config);
    };

    QObject::connect(alignCenter, &QAction::triggered, this, [changeAlignment]() { changeAlignment(Qt::AlignCenter); });
    QObject::connect(alignLeft, &QAction::triggered, this, [changeAlignment]() { changeAlignment(Qt::AlignLeft); });
    QObject::connect(alignRight, &QAction::triggered, this, [changeAlignment]() { changeAlignment(Qt::AlignRight); });

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

    menu->addAction(keepAspectRatio);
    menu->addSeparator();
    menu->addAction(alignCenter);
    menu->addAction(alignLeft);
    menu->addAction(alignRight);
    menu->addSeparator();
    menu->addAction(frontCover);
    menu->addAction(backCover);
    menu->addAction(artistCover);
    addConfigureAction(menu);

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

        extractFile->setEnabled(m_track.isValid() && !m_track.isInArchive());
        extractAs->setEnabled(m_track.isValid() && !m_track.isInArchive());

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
                         [this]() { emit requestArtworkSearch({m_track}, m_config.coverType, false); });
        QObject::connect(quickSearch, &QAction::triggered, this,
                         [this]() { emit requestArtworkSearch({m_track}, m_config.coverType, true); });
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
                         [this]() { emit requestArtworkRemoval({m_track}, m_config.coverType); });

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

void CoverWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    if(event->button() == Qt::LeftButton) {
        showArtworkViewer();
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
    }
    FyWidget::timerEvent(event);
}

void CoverWidget::paintEvent(QPaintEvent* /*event*/)
{
    QStylePainter painter{this};

    if(!m_previousScaledCover.isNull() && m_fadeProgress < 1.0) {
        painter.setOpacity(1.0 - m_fadeProgress);
        painter.drawItemPixmap(contentsRect(), Qt::AlignVCenter | m_coverAlignment, m_previousScaledCover);
        painter.setOpacity(m_fadeProgress);
    }

    painter.drawItemPixmap(contentsRect(), Qt::AlignVCenter | m_coverAlignment, m_scaledCover);
}

void CoverWidget::openConfigDialog()
{
    showConfigDialog(new CoverWidgetConfigDialog(this, this));
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

void CoverWidget::checkTrackArtwork(const Track& track)
{
    if(m_settings->value<Settings::Gui::Internal::ArtworkAutoSearch>()) {
        if(track.isValid()) {
            m_coverProvider->trackHasCover(track, m_config.coverType).then([this](const bool hasCover) {
                if(!hasCover) {
                    emit requestArtworkSearch({m_track}, m_config.coverType, true);
                }
            });
        }
    }
}
} // namespace Fooyin

#include "moc_coverwidget.cpp"
