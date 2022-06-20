/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include "core/player/playermanager.h"
#include "gui/widgets/clickablelabel.h"
#include "models/track.h"
#include "utils/utils.h"

#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QMenu>

struct StatusWidget::Private
{
    PlayerManager* playerManager;

    QHBoxLayout* layout;

    ClickableLabel* iconLabel;
    QPixmap icon{"://images/fooyin-small.png"};

    ClickableLabel* playing;

    explicit Private(PlayerManager* playerManager)
        : playerManager(playerManager)
    { }
};

StatusWidget::StatusWidget(PlayerManager* playerManager, QWidget* parent)
    : Widget(parent)
{
    p = std::make_unique<Private>(playerManager);
    setObjectName("Status Widget");

    setupUi();

    connect(p->playing, &ClickableLabel::clicked, this, &StatusWidget::labelClicked);
    connect(p->playerManager, &PlayerManager::currentTrackChanged, this, &StatusWidget::reloadStatus);
    connect(p->playerManager, &PlayerManager::playStateChanged, this, &StatusWidget::stateChanged);
}

void StatusWidget::layoutEditingMenu(QMenu* menu)
{
    Q_UNUSED(menu)
}

StatusWidget::~StatusWidget() = default;

void StatusWidget::setupUi()
{
    p->layout = new QHBoxLayout(this);
    setLayout(p->layout);

    p->layout->setContentsMargins(5, 0, 0, 0);

    p->iconLabel = new ClickableLabel(this);
    p->playing = new ClickableLabel(this);

    p->iconLabel->setPixmap(p->icon);
    p->iconLabel->setScaledContents(true);

    // p->playing->setText("Waiting for track...");

    //    Util::setMinimumWidth(p->iconLabel, "...");

    //    QPalette palette;
    //    palette.setColor(QPalette::WindowText, Qt::white);
    //    p->playing->setPalette(palette);

    p->iconLabel->setMaximumHeight(22);
    p->iconLabel->setMaximumWidth(22);

    p->layout->addWidget(p->iconLabel);
    p->layout->addWidget(p->playing);

    setMinimumHeight(25);
    //    p->playing->setMinimumHeight(25);
}

void StatusWidget::contextMenuEvent(QContextMenuEvent* event)
{
    Q_UNUSED(event)
}

void StatusWidget::labelClicked()
{
    const Player::PlayState ps = p->playerManager->playState();
    if(ps == Player::PlayState::Playing || ps == Player::PlayState::Paused)
    {
        emit clicked();
    }
}

void StatusWidget::reloadStatus()
{
    auto* track = p->playerManager->currentTrack();
    p->playing->setText(track->title());
}

void StatusWidget::stateChanged(Player::PlayState state)
{
    switch(state)
    {
        case(Player::PlayState::Stopped):
            p->playing->setText("Waiting for track...");
            break;
        case(Player::PlayState::Playing): {
            auto* track = p->playerManager->currentTrack();
            auto number = QStringLiteral("%1").arg(track->trackNumber(), 2, 10, QLatin1Char('0'));
            auto duration = QString(" (%1)").arg(Util::msToString(track->duration()));
            auto albumArtist = !track->albumArtist().isEmpty() ? " \u2022 " + track->albumArtist() : "";
            auto album = !track->album().isEmpty() ? " \u2022 " + track->album() : "";
            auto text = number + ". " + track->title() + duration + albumArtist + album;
            p->playing->setText(text);
        }
        case(Player::PlayState::Paused):
            break;
    }
}
