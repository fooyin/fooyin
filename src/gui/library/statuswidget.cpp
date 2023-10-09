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

#include "statuswidget.h"

#include <core/library/musiclibrary.h>
#include <core/player/playermanager.h>
#include <core/track.h>
#include <gui/guiconstants.h>
#include <utils/clickablelabel.h>
#include <utils/utils.h>

#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QMenu>
#include <QTimer>

namespace Fy::Gui::Widgets {
constexpr int IconSize = 50;

StatusWidget::StatusWidget(Core::Library::MusicLibrary* library, Core::Player::PlayerManager* playerManager,
                           QWidget* parent)
    : FyWidget{parent}
    , m_library{library}
    , m_playerManager{playerManager}
    , m_layout{new QHBoxLayout(this)}
    , m_iconLabel{new Utils::ClickableLabel(this)}
    , m_icon{QIcon::fromTheme(Constants::Icons::Fooyin).pixmap(IconSize)}
    , m_playing{new Utils::ClickableLabel(this)}
{
    setObjectName("Status Bar");

    m_layout->setContentsMargins(5, 0, 0, 0);

    m_iconLabel->setPixmap(m_icon);
    m_iconLabel->setScaledContents(true);

    m_iconLabel->setMaximumHeight(22);
    m_iconLabel->setMaximumWidth(22);

    m_layout->addWidget(m_iconLabel);
    m_layout->addWidget(m_playing);

    setMinimumHeight(25);

    connect(m_playing, &Utils::ClickableLabel::clicked, this, &StatusWidget::labelClicked);
    connect(m_playerManager, &Core::Player::PlayerManager::playStateChanged, this, &StatusWidget::stateChanged);
    connect(m_library, &Core::Library::MusicLibrary::scanProgress, this, &StatusWidget::scanProgressChanged);
}

QString StatusWidget::name() const
{
    return "Status";
}

void StatusWidget::contextMenuEvent(QContextMenuEvent* event)
{
    Q_UNUSED(event)
}

void StatusWidget::labelClicked()
{
    const Core::Player::PlayState ps = m_playerManager->playState();
    if(ps == Core::Player::PlayState::Playing || ps == Core::Player::PlayState::Paused) {
        emit clicked();
    }
}

void StatusWidget::stateChanged(Core::Player::PlayState state)
{
    switch(state) {
        case(Core::Player::PlayState::Stopped):
            m_playing->setText("Waiting for track...");
            break;
        case(Core::Player::PlayState::Playing): {
            const Core::Track track = m_playerManager->currentTrack();
            const auto playingText  = QString{"%1. %2 (%3) \u2022 %4 \u2022 %5"}.arg(
                QStringLiteral("%1").arg(track.trackNumber(), 2, 10, QLatin1Char('0')), track.title(),
                Utils::msToString(track.duration()), track.albumArtist(), track.album());
            m_playing->setText(playingText);
        }
        case(Core::Player::PlayState::Paused):
            break;
    }
}

void StatusWidget::scanProgressChanged(int progress)
{
    if(progress == 100) {
        QTimer::singleShot(2000, m_playing, &QLabel::clear);
    }
    const auto scanText = QString{"Scanning library: %1%"}.arg(progress);
    m_playing->setText(scanText);
}
} // namespace Fy::Gui::Widgets
