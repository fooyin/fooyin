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

#include <core/player/playercontroller.h>
#include <core/track.h>
#include <gui/coverprovider.h>
#include <gui/guiconstants.h>
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
#include <QTimer>

namespace Fooyin {
CoverWidget::CoverWidget(PlayerController* playerController, TrackSelectionController* trackSelection,
                         std::shared_ptr<TagLoader> tagLoader, SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_playerController{playerController}
    , m_trackSelection{trackSelection}
    , m_settings{settings}
    , m_coverProvider{new CoverProvider(std::move(tagLoader), settings, this)}
    , m_displayOption{static_cast<CoverDisplay>(m_settings->value<Settings::Gui::Internal::TrackCoverDisplayOption>())}
    , m_coverType{Track::Cover::Front}
    , m_keepAspectRatio{true}
    , m_resizeTimer{new QTimer(this)}
    , m_coverLabel{new QLabel(this)}
{
    setObjectName(CoverWidget::name());

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_coverLabel);

    m_resizeTimer->setSingleShot(true);
    m_coverLabel->setMinimumSize(100, 100);

    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this, [this]() { reloadCover(); });
    QObject::connect(m_trackSelection, &TrackSelectionController::selectionChanged, this, [this]() { reloadCover(); });
    QObject::connect(
        m_coverProvider, &CoverProvider::coverAdded, this, [this]() { reloadCover(); }, Qt::QueuedConnection);
    QObject::connect(m_resizeTimer, &QTimer::timeout, this, &CoverWidget::rescaleCover);

    m_settings->subscribe<Settings::Gui::Internal::TrackCoverDisplayOption>(this, [this](const int option) {
        m_displayOption = static_cast<CoverDisplay>(option);
        reloadCover();
    });
    m_settings->subscribe<Settings::Gui::IconTheme>(this, [this]() { reloadCover(); });

    reloadCover();
}

QString CoverWidget::name() const
{
    return tr("Artwork Panel");
}

QString CoverWidget::layoutName() const
{
    return QStringLiteral("ArtworkPanel");
}

void CoverWidget::saveLayoutData(QJsonObject& layout)
{
    layout[QStringLiteral("CoverType")]       = static_cast<int>(m_coverType);
    layout[QStringLiteral("KeepAspectRatio")] = m_keepAspectRatio;
}

void CoverWidget::loadLayoutData(const QJsonObject& layout)
{
    if(layout.contains(QStringLiteral("CoverType"))) {
        m_coverType = static_cast<Track::Cover>(layout.value(QStringLiteral("CoverType")).toInt());
    }
    if(layout.contains(QStringLiteral("KeepAspectRatio"))) {
        m_keepAspectRatio = layout.value(QStringLiteral("KeepAspectRatio")).toBool();
    }
}

void CoverWidget::resizeEvent(QResizeEvent* event)
{
    if(!m_cover.isNull()) {
        m_resizeTimer->start(5);
    }

    QWidget::resizeEvent(event);
}

void CoverWidget::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* keepAspectRatio = new QAction(tr("Keep Aspect Ratio"), this);

    keepAspectRatio->setCheckable(true);
    keepAspectRatio->setChecked(m_keepAspectRatio);

    QObject::connect(keepAspectRatio, &QAction::triggered, this, [this](bool checked) {
        m_keepAspectRatio = checked;
        rescaleCover();
    });

    auto* coverGroup = new QActionGroup(menu);

    auto* frontCover  = new QAction(tr("Front Cover"), coverGroup);
    auto* backCover   = new QAction(tr("Back Cover"), coverGroup);
    auto* artistCover = new QAction(tr("Artist"), coverGroup);

    frontCover->setCheckable(true);
    backCover->setCheckable(true);
    artistCover->setCheckable(true);

    frontCover->setChecked(m_coverType == Track::Cover::Front);
    backCover->setChecked(m_coverType == Track::Cover::Back);
    artistCover->setChecked(m_coverType == Track::Cover::Artist);

    QObject::connect(frontCover, &QAction::triggered, this, [this]() {
        m_coverType = Track::Cover::Front;
        reloadCover();
    });
    QObject::connect(backCover, &QAction::triggered, this, [this]() {
        m_coverType = Track::Cover::Back;
        reloadCover();
    });
    QObject::connect(artistCover, &QAction::triggered, this, [this]() {
        m_coverType = Track::Cover::Artist;
        reloadCover();
    });

    menu->addAction(keepAspectRatio);
    menu->addSeparator();
    menu->addAction(frontCover);
    menu->addAction(backCover);
    menu->addAction(artistCover);

    menu->popup(event->globalPos());
}

void CoverWidget::rescaleCover() const
{
    const auto aspectRatio = m_keepAspectRatio ? Qt::KeepAspectRatio : Qt::IgnoreAspectRatio;
    m_coverLabel->setPixmap(m_cover.scaled(size(), aspectRatio, Qt::SmoothTransformation));
}

void CoverWidget::reloadCover()
{
    Track track;

    if(m_displayOption == CoverDisplay::PreferSelection && m_trackSelection->hasTracks()) {
        track = m_trackSelection->selectedTrack();
    }
    else {
        track = m_playerController->currentTrack();
    }

    m_cover = m_coverProvider->trackCover(track, m_coverType);
    rescaleCover();
}
} // namespace Fooyin

#include "moc_coverwidget.cpp"
