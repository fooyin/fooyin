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
struct CoverWidget::Private
{
    CoverWidget* self;

    PlayerController* playerController;
    TrackSelectionController* trackSelection;
    CoverProvider* coverProvider;

    QLabel* coverLabel;
    QPixmap cover;

    Private(CoverWidget* self_, PlayerController* playerController_, TrackSelectionController* trackSelection_)
        : self{self_}
        , playerController{playerController_}
        , trackSelection{trackSelection_}
        , coverProvider{new CoverProvider(self)}
        , coverLabel{new QLabel(self)}
    {
        auto* layout = new QVBoxLayout(self);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setAlignment(Qt::AlignCenter);
        layout->addWidget(coverLabel);

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
        cover = coverProvider->trackCover(track);
        rescaleCover();
    }
};

CoverWidget::CoverWidget(PlayerController* playerController, TrackSelectionController* trackSelection, QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<Private>(this, playerController, trackSelection)}
{
    setObjectName(CoverWidget::name());

    QObject::connect(p->playerController, &PlayerController::currentTrackChanged, this,
                     [this](const Track& track) { p->reloadCover(track); });
    QObject::connect(
        p->coverProvider, &CoverProvider::coverAdded, this, [this](const Track& track) { p->reloadCover(track); },
        Qt::QueuedConnection);

    p->reloadCover(p->playerController->currentTrack());
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
