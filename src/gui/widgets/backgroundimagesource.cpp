/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include <gui/widgets/backgroundimagesource.h>

#include <core/player/playercontroller.h>
#include <gui/coverprovider.h>
#include <gui/trackselectioncontroller.h>

#include <QPixmap>

namespace Fooyin {
class BackgroundImageSourcePrivate
{
public:
    BackgroundImageSourcePrivate(BackgroundImageSource* self, CoverProvider* coverProvider,
                                 PlayerController* playerController, TrackSelectionController* selectionController)
        : m_self{self}
        , m_coverProvider{coverProvider}
        , m_playerController{playerController}
        , m_selectionController{selectionController}
    { }

    void reloadCover()
    {
        if(static_cast<BackgroundImageMode>(m_config.imageMode) != BackgroundImageMode::AlbumCover) {
            return;
        }

        const int requestId{++m_requestId};
        const auto source = preferredTrackSource(m_config.trackPreference, m_playerController->playState(),
                                                 m_playerController->playbackStarted());
        if(source == PreferredTrackSource::None) {
            m_coverTrack = {};
            Q_EMIT m_self->pixmapChanged({});
            return;
        }

        const Track playingTrack = m_playerController->currentTrack();
        const Track selectedTrack
            = m_selectionController->hasDisplayTracks() ? m_selectionController->displayTrack() : Track{};

        Track coverTrack;
        if(source == PreferredTrackSource::Selected) {
            coverTrack = selectedTrack.isValid() ? selectedTrack : playingTrack;
        }
        else {
            coverTrack = playingTrack.isValid() ? playingTrack : selectedTrack;
        }

        if(!coverTrack.isValid()) {
            m_coverTrack = {};
            Q_EMIT m_self->pixmapChanged({});
            return;
        }

        m_coverTrack = coverTrack;
        m_coverProvider->trackCoverFull(coverTrack, static_cast<Track::Cover>(m_config.coverType))
            .then(m_self, [this, requestId](const QPixmap& cover) {
                if(requestId == m_requestId) {
                    Q_EMIT m_self->pixmapChanged(cover);
                }
            });
    }

    BackgroundImageSource* m_self;
    CoverProvider* m_coverProvider;
    PlayerController* m_playerController;
    TrackSelectionController* m_selectionController;
    BackgroundConfig m_config;
    Track m_coverTrack;
    int m_requestId{0};
};

BackgroundImageSource::BackgroundImageSource(CoverProvider* coverProvider, PlayerController* playerController,
                                             TrackSelectionController* selectionController, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<BackgroundImageSourcePrivate>(this, coverProvider, playerController, selectionController)}
{
    const auto reloadCover = [this]() {
        p->reloadCover();
    };

    QObject::connect(p->m_playerController, &PlayerController::currentTrackChanged, this, reloadCover);
    QObject::connect(p->m_playerController, &PlayerController::currentTrackUpdated, this, reloadCover);
    QObject::connect(p->m_playerController, &PlayerController::playStateChanged, this, reloadCover);
    QObject::connect(p->m_selectionController, &TrackSelectionController::displaySelectionChanged, this, reloadCover);
    QObject::connect(p->m_coverProvider, &CoverProvider::coverAdded, this, [this, reloadCover](const Track& track) {
        if(p->m_coverTrack.isValid() && p->m_coverTrack.sameIdentityAs(track)) {
            reloadCover();
        }
    });
}

BackgroundImageSource::~BackgroundImageSource() = default;

void BackgroundImageSource::setConfig(const BackgroundConfig& config)
{
    auto validated{config};
    validated.normalise();

    const auto previous    = std::exchange(p->m_config, validated);
    const bool modeChanged = previous.imageMode != p->m_config.imageMode;

    switch(static_cast<BackgroundImageMode>(p->m_config.imageMode)) {
        case BackgroundImageMode::AlbumCover:
            if(modeChanged || previous.coverType != p->m_config.coverType
               || previous.trackPreference != p->m_config.trackPreference || !p->m_coverTrack.isValid()) {
                p->reloadCover();
            }
            break;
        case BackgroundImageMode::Custom:
            if(modeChanged || previous.customImage != p->m_config.customImage) {
                ++p->m_requestId;
                p->m_coverTrack = {};
                Q_EMIT pixmapChanged(QPixmap{p->m_config.customImage});
            }
            break;
        case BackgroundImageMode::None:
            if(modeChanged) {
                ++p->m_requestId;
                p->m_coverTrack = {};
                Q_EMIT pixmapChanged({});
            }
            break;
    }
}
} // namespace Fooyin

#include "gui/widgets/moc_backgroundimagesource.cpp"
