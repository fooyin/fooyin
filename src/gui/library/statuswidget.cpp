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

#include "gui/widgets/clickablelabel.h"

#include <core/models/track.h>
#include <core/player/playermanager.h>
#include <core/plugins/pluginmanager.h>
#include <utils/utils.h>

#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QMenu>

namespace Gui::Widgets {
StatusWidget::StatusWidget(Core::Player::PlayerManager* playerManager, QWidget* parent)
    : FyWidget{parent}
    , m_playerManager{playerManager}
    , m_layout{new QHBoxLayout(this)}
    , m_iconLabel{new ClickableLabel(this)}
    , m_icon{"://images/fooyin-small.png"}
    , m_playing{new ClickableLabel(this)}
{
    setObjectName("Status Bar");

    setupUi();

    connect(m_playing, &ClickableLabel::clicked, this, &StatusWidget::labelClicked);
    connect(m_playerManager, &Core::Player::PlayerManager::currentTrackChanged, this, &StatusWidget::reloadStatus);
    connect(m_playerManager, &Core::Player::PlayerManager::playStateChanged, this, &StatusWidget::stateChanged);
}

QString StatusWidget::name() const
{
    return "Status";
}

StatusWidget::~StatusWidget() = default;

void StatusWidget::setupUi()
{
    m_layout->setContentsMargins(5, 0, 0, 0);

    m_iconLabel->setPixmap(m_icon);
    m_iconLabel->setScaledContents(true);

    // m_playing->setText("Waiting for track...");

    //    Utils::setMinimumWidth(m_iconLabel, "...");

    //    QPalette palette;
    //    palette.setColor(QPalette::WindowText, Qt::white);
    //    m_playing->setPalette(palette);

    m_iconLabel->setMaximumHeight(22);
    m_iconLabel->setMaximumWidth(22);

    m_layout->addWidget(m_iconLabel);
    m_layout->addWidget(m_playing);

    setMinimumHeight(25);
    //    m_playing->setMinimumHeight(25);
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

void StatusWidget::reloadStatus()
{
    auto* track = m_playerManager->currentTrack();
    m_playing->setText(track->title());
}

void StatusWidget::stateChanged(Core::Player::PlayState state)
{
    switch(state) {
        case(Core::Player::PlayState::Stopped):
            m_playing->setText("Waiting for track...");
            break;
        case(Core::Player::PlayState::Playing): {
            auto* track      = m_playerManager->currentTrack();
            auto number      = QStringLiteral("%1").arg(track->trackNumber(), 2, 10, QLatin1Char('0'));
            auto duration    = QString(" (%1)").arg(Utils::msToString(track->duration()));
            auto albumArtist = !track->albumArtist().isEmpty() ? " \u2022 " + track->albumArtist() : "";
            auto album       = !track->album().isEmpty() ? " \u2022 " + track->album() : "";
            auto text        = number + ". " + track->title() + duration + albumArtist + album;
            m_playing->setText(text);
        }
        case(Core::Player::PlayState::Paused):
            break;
    }
}
} // namespace Gui::Widgets
