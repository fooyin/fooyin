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
                         std::shared_ptr<AudioLoader> audioLoader, SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_playerController{playerController}
    , m_trackSelection{trackSelection}
    , m_settings{settings}
    , m_coverProvider{new CoverProvider(std::move(audioLoader), settings, this)}
    , m_displayOption{static_cast<SelectionDisplay>(
          m_settings->value<Settings::Gui::Internal::TrackCoverDisplayOption>())}
    , m_coverType{Track::Cover::Front}
    , m_coverAlignment{Qt::AlignCenter}
    , m_keepAspectRatio{true}
    , m_resizeTimer{new QTimer(this)}
    , m_coverLayout{new QVBoxLayout(this)}
    , m_coverLabel{new QLabel(this)}
{
    setObjectName(CoverWidget::name());

    m_coverLayout->setContentsMargins(0, 0, 0, 0);
    m_coverLayout->setAlignment(m_coverAlignment);
    m_coverLayout->addWidget(m_coverLabel);

    m_resizeTimer->setSingleShot(true);
    m_coverLabel->setMinimumSize(100, 100);

    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this, [this]() { reloadCover(); });
    QObject::connect(m_trackSelection, &TrackSelectionController::selectionChanged, this, [this]() { reloadCover(); });
    QObject::connect(
        m_coverProvider, &CoverProvider::coverAdded, this, [this]() { reloadCover(); }, Qt::QueuedConnection);
    QObject::connect(m_resizeTimer, &QTimer::timeout, this, &CoverWidget::rescaleCover);

    m_settings->subscribe<Settings::Gui::Internal::TrackCoverDisplayOption>(this, [this](const int option) {
        m_displayOption = static_cast<SelectionDisplay>(option);
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
    layout[u"CoverType"]       = static_cast<int>(m_coverType);
    layout[u"CoverAlignment"]  = static_cast<int>(m_coverAlignment);
    layout[u"KeepAspectRatio"] = m_keepAspectRatio;
}

void CoverWidget::loadLayoutData(const QJsonObject& layout)
{
    if(layout.contains(u"CoverType")) {
        m_coverType = static_cast<Track::Cover>(layout.value(u"CoverType").toInt());
    }
    if(layout.contains(u"CoverAlignment")) {
        m_coverAlignment = static_cast<Qt::Alignment>(layout.value(u"CoverAlignment").toInt());
    }
    if(layout.contains(u"KeepAspectRatio")) {
        m_keepAspectRatio = layout.value(u"KeepAspectRatio").toBool();
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

    auto* keepAspectRatio = new QAction(tr("Keep aspect ratio"), this);

    keepAspectRatio->setCheckable(true);
    keepAspectRatio->setChecked(m_keepAspectRatio);

    QObject::connect(keepAspectRatio, &QAction::triggered, this, [this](bool checked) {
        m_keepAspectRatio = checked;
        rescaleCover();
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

    QObject::connect(alignCenter, &QAction::triggered, this, [this]() {
        m_coverAlignment = Qt::AlignCenter;
        reloadCover();
    });
    QObject::connect(alignLeft, &QAction::triggered, this, [this]() {
        m_coverAlignment = Qt::AlignLeft;
        reloadCover();
    });
    QObject::connect(alignRight, &QAction::triggered, this, [this]() {
        m_coverAlignment = Qt::AlignRight;
        reloadCover();
    });

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
    menu->addAction(alignCenter);
    menu->addAction(alignLeft);
    menu->addAction(alignRight);
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

    if(m_displayOption == SelectionDisplay::PreferSelection && m_trackSelection->hasTracks()) {
        track = m_trackSelection->selectedTrack();
    }
    else {
        track = m_playerController->currentTrack();
    }

    m_coverLayout->setAlignment(m_coverAlignment);
    m_cover = m_coverProvider->trackCover(track, m_coverType);
    rescaleCover();
}
} // namespace Fooyin

#include "moc_coverwidget.cpp"
