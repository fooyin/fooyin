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
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QStyleOption>
#include <QStylePainter>
#include <QTimer>
#include <QTimerEvent>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;

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
    , m_coverType{Track::Cover::Front}
    , m_coverAlignment{Qt::AlignCenter}
    , m_keepAspectRatio{true}
    , m_noCover{m_coverProvider->placeholderCover()}
{
    setObjectName(CoverWidget::name());

    m_coverProvider->setUsePlaceholder(false);

    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this, &CoverWidget::reloadCover);
    QObject::connect(m_trackSelection, &TrackSelectionController::selectionChanged, this, &CoverWidget::reloadCover);
    QObject::connect(m_coverProvider, &CoverProvider::coverAdded, this, &CoverWidget::reloadCover,
                     Qt::QueuedConnection);

    m_settings->subscribe<Settings::Gui::Internal::TrackCoverDisplayOption>(this, [this](const int option) {
        m_displayOption = static_cast<SelectionDisplay>(option);
        reloadCover();
    });
    m_settings->subscribe<Settings::Gui::IconTheme>(this, &CoverWidget::reloadCover);
    m_settings->subscribe<Settings::Gui::Theme>(this, &CoverWidget::reloadCover);
    m_settings->subscribe<Settings::Gui::Style>(this, &CoverWidget::reloadCover);

    reloadCover();
}

void CoverWidget::rescaleCover()
{
    const auto aspectRatio = m_keepAspectRatio ? Qt::KeepAspectRatio : Qt::IgnoreAspectRatio;
    const double dpr       = devicePixelRatioF();
    const QSize scaledSize = size() * dpr;

    const QPixmap& cover = m_cover.isNull() ? m_noCover : m_cover;

    m_scaledCover = cover.scaled(scaledSize, aspectRatio, Qt::SmoothTransformation);
    m_scaledCover.setDevicePixelRatio(dpr);

    update();
}

void CoverWidget::reloadCover()
{
    m_track = {};

    if(m_displayOption == SelectionDisplay::PreferSelection && m_trackSelection->hasTracks()) {
        m_track = m_trackSelection->selectedTrack();
    }
    else {
        m_track = m_playerController->currentTrack();
    }

    m_cover = m_coverProvider->trackCover(m_track, m_coverType);
    // Delay showing cover so we don't display the placeholder if still loading
    // TODO: Implement fading between cover changes
    QTimer::singleShot(200, this, &CoverWidget::rescaleCover);
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
    layout["CoverType"_L1]       = static_cast<int>(m_coverType);
    layout["CoverAlignment"_L1]  = static_cast<int>(m_coverAlignment);
    layout["KeepAspectRatio"_L1] = m_keepAspectRatio;
}

void CoverWidget::loadLayoutData(const QJsonObject& layout)
{
    if(layout.contains("CoverType"_L1)) {
        m_coverType = static_cast<Track::Cover>(layout.value("CoverType"_L1).toInt());
    }
    if(layout.contains("CoverAlignment"_L1)) {
        m_coverAlignment = static_cast<Qt::Alignment>(layout.value("CoverAlignment"_L1).toInt());
    }
    if(layout.contains("KeepAspectRatio"_L1)) {
        m_keepAspectRatio = layout.value("KeepAspectRatio"_L1).toBool();
    }
}

void CoverWidget::resizeEvent(QResizeEvent* event)
{
    if(!m_cover.isNull()) {
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
    keepAspectRatio->setChecked(m_keepAspectRatio);

    QObject::connect(keepAspectRatio, &QAction::triggered, this, [this](bool checked) {
        m_keepAspectRatio = checked;
        rescaleCover();
        update();
    });

    auto* alignmentGroup = new QActionGroup(menu);

    auto* alignCenter = new QAction(tr("Align to centre"), alignmentGroup);
    auto* alignLeft   = new QAction(tr("Align to left"), alignmentGroup);
    auto* alignRight  = new QAction(tr("Align to right"), alignmentGroup);

    alignCenter->setCheckable(true);
    alignLeft->setCheckable(true);
    alignRight->setCheckable(true);

    alignCenter->setChecked(m_coverAlignment == Qt::AlignCenter);
    alignLeft->setChecked(m_coverAlignment == Qt::AlignLeft);
    alignRight->setChecked(m_coverAlignment == Qt::AlignRight);

    auto changeAlignment = [this](Qt::Alignment alignment) {
        m_coverAlignment = alignment;
        reloadCover();
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

    frontCover->setChecked(m_coverType == Track::Cover::Front);
    backCover->setChecked(m_coverType == Track::Cover::Back);
    artistCover->setChecked(m_coverType == Track::Cover::Artist);

    auto reload = [this](Track::Cover type) {
        m_coverType = type;
        reloadCover();
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

    if(m_track.isValid()) {
        const auto canWrite = [this]() {
            return !m_track.hasCue() && !m_track.isInArchive() && m_audioLoader->canWriteMetadata(m_track);
        };
        const auto canWriteCover = [this, canWrite]() {
            return canWrite()
                || m_settings->value<Settings::Gui::Internal::ArtworkSaveMethods>()
                           .value<ArtworkSaveMethods>()
                           .value(m_coverType)
                           .method
                       == ArtworkSaveMethod::Directory;
        };

        auto* search      = new QAction(tr("Search for artwork…"), menu);
        auto* quickSearch = new QAction(tr("Quicksearch for artwork"), menu);
        auto* remove      = new QAction(tr("Remove artwork"), menu);

        search->setEnabled(canWriteCover());
        quickSearch->setEnabled(canWriteCover());
        remove->setEnabled(canWriteCover());

        if(m_coverType == Track::Cover::Artist) {
            // Only support front and back cover for now
            search->setDisabled(true);
            quickSearch->setDisabled(true);
            remove->setDisabled(true);
        }

        if(m_cover.isNull()) {
            remove->setDisabled(true);
        }

        QObject::connect(search, &QAction::triggered, this,
                         [this]() { emit requestArtworkSearch({m_track}, m_coverType, false); });
        QObject::connect(quickSearch, &QAction::triggered, this,
                         [this]() { emit requestArtworkSearch({m_track}, m_coverType, true); });
        QObject::connect(remove, &QAction::triggered, this,
                         [this]() { emit requestArtworkRemoval({m_track}, m_coverType); });

        menu->addSeparator();
        menu->addAction(search);
        // menu->addAction(quickSearch);
        menu->addAction(remove);
    }

    menu->popup(event->globalPos());
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
    painter.drawItemPixmap(contentsRect(), static_cast<int>(Qt::AlignVCenter | m_coverAlignment), m_scaledCover);
}
} // namespace Fooyin

#include "moc_coverwidget.cpp"
