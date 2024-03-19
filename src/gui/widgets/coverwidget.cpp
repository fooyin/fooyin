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

#include <QHBoxLayout>
#include <QLabel>

namespace Fooyin {
CoverWidget::CoverWidget(PlayerController* playerController, TrackSelectionController* trackSelection, QWidget* parent)
    : FyWidget{parent}
    , m_playerController{playerController}
    , m_trackSelection{trackSelection}
    , m_coverProvider{new CoverProvider(this)}
    , m_coverLabel{new QLabel(this)}
{
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

void CoverWidget::resizeEvent(QResizeEvent* event)
{
    if(!m_cover.isNull()) {
        rescaleCover();
    }

    QWidget::resizeEvent(event);
}

void CoverWidget::rescaleCover() const
{
    const QSize scale = size() * 4;
    m_coverLabel->setPixmap(
        m_cover.scaled(scale, Qt::KeepAspectRatio).scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void CoverWidget::reloadCover(const Track& track)
{
    m_cover = m_coverProvider->trackCover(track);
    rescaleCover();
}
} // namespace Fooyin

#include "moc_coverwidget.cpp"
