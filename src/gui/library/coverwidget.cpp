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

namespace Fy::Gui::Widgets {
CoverWidget::CoverWidget(Core::Library::MusicLibrary* library, Core::Player::PlayerManager* playerManager,
                         QWidget* parent)
    : FyWidget{parent}
    , m_library{library}
    , m_playerManager{playerManager}
    , m_layout{new QHBoxLayout(this)}
    , m_coverLabel{new QLabel(this)}
{
    setObjectName("Artwork");

    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setAlignment(Qt::AlignCenter);
    m_coverLabel->setMinimumSize(100, 100);
    m_layout->addWidget(m_coverLabel);

    connect(m_playerManager, &Core::Player::PlayerManager::currentTrackChanged, this, &CoverWidget::reloadCover);
    //    connect(m_library, &Core::Library::MusicLibrary::tracksChanged, this, &CoverWidget::reloadCover);

    reloadCover({});
}

QString CoverWidget::name() const
{
    return "Artwork";
}

void CoverWidget::resizeEvent(QResizeEvent* e)
{
    if(!m_cover.isNull()) {
        rescaleCover();
    }
    QWidget::resizeEvent(e);
}

void CoverWidget::reloadCover(const Core::Track& track)
{
    if(track.isValid()) {
        m_cover = m_coverProvider.trackCover(track);
        if(!m_cover.isNull()) {
            rescaleCover();
        }
    }
}

void CoverWidget::rescaleCover()
{
    const int w       = width();
    const int h       = height();
    const QSize scale = {w * 4, h * 4};
    m_coverLabel->setPixmap(
        m_cover.scaled(scale, Qt::KeepAspectRatio).scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}
} // namespace Fy::Gui::Widgets
