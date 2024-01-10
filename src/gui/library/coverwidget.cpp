/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

#include <core/player/playermanager.h>
#include <core/track.h>
#include <gui/coverprovider.h>
#include <gui/guiconstants.h>
#include <gui/trackselectioncontroller.h>

#include <QHBoxLayout>
#include <QLabel>

namespace Fooyin {
struct CoverWidget::Private
{
    CoverWidget* self;

    PlayerManager* playerManager;
    TrackSelectionController* trackSelection;
    CoverProvider* coverProvider;

    QLabel* coverLabel;
    QPixmap cover;

    Private(CoverWidget* self, PlayerManager* playerManager, TrackSelectionController* trackSelection)
        : self{self}
        , playerManager{playerManager}
        , trackSelection{trackSelection}
        , coverProvider{new CoverProvider(self)}
        , coverLabel{new QLabel(self)}
    {
        coverLabel->setMinimumSize(100, 100);
    }

    void rescaleCover() const
    {
        const QSize scale = self->size() * 4;
        coverLabel->setPixmap(cover.scaled(scale, Qt::KeepAspectRatio)
                                  .scaled(self->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    void reloadCover(const Track& track)
    {
        if(track.isValid()) {
            cover = coverProvider->trackCover(track);
            if(!cover.isNull()) {
                rescaleCover();
            }
        }
        else {
            cover.load(Constants::NoCover);
            rescaleCover();
        }
    }
};

CoverWidget::CoverWidget(PlayerManager* playerManager, TrackSelectionController* trackSelection, QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<Private>(this, playerManager, trackSelection)}
{
    setObjectName(CoverWidget::name());

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setAlignment(Qt::AlignCenter);
    layout->addWidget(p->coverLabel);

    QObject::connect(p->playerManager, &PlayerManager::currentTrackChanged, this,
                     [this](const Track& track) { p->reloadCover(track); });
    QObject::connect(p->coverProvider, &CoverProvider::coverAdded, this,
                     [this](const Track& track) { p->reloadCover(track); });

    p->reloadCover(p->playerManager->currentTrack());
}

CoverWidget::~CoverWidget() = default;

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
    if(!p->cover.isNull()) {
        p->rescaleCover();
    }
    QWidget::resizeEvent(event);
}
} // namespace Fooyin

#include "moc_coverwidget.cpp"
