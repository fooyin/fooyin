/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include <core/library/musiclibrary.h>
#include <core/models/track.h>
#include <core/player/playermanager.h>

#include <QHBoxLayout>
#include <QLabel>

namespace Gui::Widgets {
CoverWidget::CoverWidget(Core::Library::MusicLibrary* library, Core::Player::PlayerManager* playerManager,
                         QWidget* parent)
    : FyWidget{parent}
    , m_library{library}
    , m_playerManager{playerManager}
    , m_layout{new QHBoxLayout(this)}
    , m_coverLabel{new QLabel(this)}
{
    setObjectName("Artwork");
    setupUi();

    connect(m_playerManager, &Core::Player::PlayerManager::currentTrackChanged, this, &CoverWidget::reloadCover);
    connect(m_library, &Core::Library::MusicLibrary::tracksChanged, this, &CoverWidget::reloadCover);
    connect(m_library, &Core::Library::MusicLibrary::tracksSelChanged, this, &CoverWidget::reloadCover);

    reloadCover();
}

QString CoverWidget::name() const
{
    return "Artwork";
}

void CoverWidget::setupUi()
{
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setAlignment(Qt::AlignCenter);
    setAutoFillBackground(true);

    m_coverLabel->setMinimumSize(100, 100);
    m_layout->addWidget(m_coverLabel);
}

void CoverWidget::resizeEvent(QResizeEvent* e)
{
    if(m_hasCover) {
        m_coverLabel->setPixmap(m_cover.scaled(width(), height(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    QWidget::resizeEvent(e);
}

void CoverWidget::reloadCover()
{
    QString coverPath = "";
    if(!m_library->tracks().empty()) {
        const Core::Player::PlayState state = m_playerManager->playState();
        if(state == Core::Player::PlayState::Playing || state == Core::Player::PlayState::Paused) {
            Core::Track* track = m_playerManager->currentTrack();
            if(track) {
                coverPath = track->coverPath();
            }
        }
        else if(m_library->selectedTracks().empty()) {
            Core::Track* track = m_library->tracks().front();
            if(track) {
                coverPath = track->coverPath();
            }
        }
        else {
            Core::Track* track = m_library->selectedTracks().front();
            if(track) {
                coverPath = track->coverPath();
            }
        }
    }

    if(coverPath.isEmpty()) {
        coverPath = "://images/nocover.png";
        //        setAutoFillBackground(true);
        //        QPalette palette = m_coverLabel->palette();
        //        palette.setColor(m_coverLabel->backgroundRole(), palette.base().color());
        //        m_coverLabel->setPalette(palette);
    }

    if(coverPath != m_coverPath) {
        m_coverPath = coverPath;
        m_cover.load(coverPath);
        if(!m_cover.isNull()) {
            m_coverLabel->setPixmap(m_cover.scaled(width(), height(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            m_hasCover = true;
        }
    }
}
} // namespace Gui::Widgets
