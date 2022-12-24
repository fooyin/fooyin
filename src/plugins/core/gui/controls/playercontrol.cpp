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

#include "playercontrol.h"

#include "core/constants.h"
#include "core/gui/widgets/clickablelabel.h"

#include <QHBoxLayout>
#include <QPainter>
#include <utils/utils.h>

struct PlayerControl::Private
{
    QHBoxLayout* layout;

    ClickableLabel* stop;
    ClickableLabel* prev;
    ClickableLabel* play;
    ClickableLabel* next;
};

PlayerControl::PlayerControl(QWidget* parent)
    : QWidget(parent)
    , p(std::make_unique<Private>())
{
    setupUi();

    connect(p->stop, &ClickableLabel::clicked, this, &PlayerControl::stopClicked);
    connect(p->prev, &ClickableLabel::clicked, this, &PlayerControl::prevClicked);
    connect(p->play, &ClickableLabel::clicked, this, &PlayerControl::pauseClicked);
    connect(p->next, &ClickableLabel::clicked, this, &PlayerControl::nextClicked);
}

PlayerControl::~PlayerControl() = default;

void PlayerControl::setupUi()
{
    p->layout = new QHBoxLayout(this);

    p->layout->setSizeConstraint(QLayout::SetFixedSize);
    p->layout->setSpacing(10);
    p->layout->setContentsMargins(10, 0, 0, 0);

    p->stop = new ClickableLabel(this);
    p->prev = new ClickableLabel(this);
    p->play = new ClickableLabel(this);
    p->next = new ClickableLabel(this);

    p->layout->addWidget(p->stop, 0, Qt::AlignVCenter);
    p->layout->addWidget(p->prev, 0, Qt::AlignVCenter);
    p->layout->addWidget(p->play, 0, Qt::AlignVCenter);
    p->layout->addWidget(p->next, 0, Qt::AlignVCenter);

    QFont font = QFont(Core::Constants::IconFont, 12);
    setFont(font);

    p->stop->setText(Core::Constants::Icons::Stop);
    p->prev->setText(Core::Constants::Icons::Prev);
    p->play->setText(Core::Constants::Icons::Play);
    p->next->setText(Core::Constants::Icons::Next);

    Util::setMinimumWidth(p->play, Core::Constants::Icons::Play);

    setEnabled(false);
}

void PlayerControl::stateChanged(Player::PlayState state)
{
    switch(state) {
        case(Player::PlayState::Stopped):
            setEnabled(false);
            return p->play->setText(Core::Constants::Icons::Play);
        case(Player::PlayState::Playing):
            setEnabled(true);
            return p->play->setText(Core::Constants::Icons::Pause);
        case(Player::PlayState::Paused):
            return p->play->setText(Core::Constants::Icons::Play);
    }
}
