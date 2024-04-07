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
#include <gui/trackselectioncontroller.h>

#include <QAction>
#include <QActionGroup>
#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>

namespace Fooyin {
CoverWidget::CoverWidget(PlayerController* playerController, TrackSelectionController* trackSelection, QWidget* parent)
    : FyWidget{parent}
    , m_playerController{playerController}
    , m_trackSelection{trackSelection}
    , m_coverProvider{new CoverProvider(this)}
    , m_coverType{Track::Cover::Front}
    , m_keepAspectRatio{true}
    , m_coverLabel{new QLabel(this)}
{
    Q_UNUSED(m_trackSelection)

    setObjectName(CoverWidget::name());

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_coverLabel);

    m_coverLabel->setMinimumSize(100, 100);

    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this,
                     [this](const Track& track) { reloadCover(track); });
    QObject::connect(
        m_coverProvider, &CoverProvider::coverAdded, this, [this](const Track& track) { reloadCover(track); },
        Qt::QueuedConnection);

    reloadCover(m_playerController->currentTrack());
}

QString CoverWidget::name() const
{
    return QStringLiteral("Artwork Panel");
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
        rescaleCover();
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

    auto* frontCover  = new QAction(tr("Font"), coverGroup);
    auto* backCover   = new QAction(tr("Back"), coverGroup);
    auto* artistCover = new QAction(tr("Artist"), coverGroup);

    frontCover->setCheckable(true);
    backCover->setCheckable(true);
    artistCover->setCheckable(true);

    frontCover->setChecked(m_coverType == Track::Cover::Front);
    backCover->setChecked(m_coverType == Track::Cover::Back);
    artistCover->setChecked(m_coverType == Track::Cover::Artist);

    QObject::connect(frontCover, &QAction::triggered, this, [this]() {
        m_coverType = Track::Cover::Front;
        reloadCover(m_playerController->currentTrack());
    });
    QObject::connect(backCover, &QAction::triggered, this, [this]() {
        m_coverType = Track::Cover::Back;
        reloadCover(m_playerController->currentTrack());
    });
    QObject::connect(artistCover, &QAction::triggered, this, [this]() {
        m_coverType = Track::Cover::Artist;
        reloadCover(m_playerController->currentTrack());
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
    const QSize scale      = size() * 4;
    const auto aspectRatio = m_keepAspectRatio ? Qt::KeepAspectRatio : Qt::IgnoreAspectRatio;
    m_coverLabel->setPixmap(m_cover.scaled(scale, aspectRatio).scaled(size(), aspectRatio, Qt::SmoothTransformation));
}

void CoverWidget::reloadCover(const Track& track)
{
    m_cover = m_coverProvider->trackCover(track, m_coverType);
    rescaleCover();
}
} // namespace Fooyin

#include "moc_coverwidget.cpp"
