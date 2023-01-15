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
#include "core/gui/widgets/comboicon.h"

#include <QHBoxLayout>

namespace Core::Widgets {
struct PlayerControl::Private
{
    QHBoxLayout* layout;

    ComboIcon* stop;
    ComboIcon* prev;
    ComboIcon* play;
    ComboIcon* next;

    QSize labelSize{20, 20};
};

PlayerControl::PlayerControl(QWidget* parent)
    : QWidget(parent)
    , p(std::make_unique<Private>())
{
    setupUi();

    connect(p->stop, &ComboIcon::clicked, this, &PlayerControl::stopClicked);
    connect(p->prev, &ComboIcon::clicked, this, &PlayerControl::prevClicked);
    connect(p->play, &ComboIcon::clicked, this, &PlayerControl::pauseClicked);
    connect(p->next, &ComboIcon::clicked, this, &PlayerControl::nextClicked);
}

PlayerControl::~PlayerControl() = default;

void PlayerControl::setupUi()
{
    p->layout = new QHBoxLayout(this);

    p->layout->setSizeConstraint(QLayout::SetFixedSize);
    p->layout->setSpacing(10);
    p->layout->setContentsMargins(10, 0, 0, 0);

    p->stop = new ComboIcon(Core::Constants::Icons::Stop, this);
    p->prev = new ComboIcon(Core::Constants::Icons::Prev, this);
    p->next = new ComboIcon(Core::Constants::Icons::Next, this);
    p->play = new ComboIcon(Core::Constants::Icons::Play, this);
    p->play->addPixmap(Core::Constants::Icons::Pause);

    p->stop->setMaximumSize(p->labelSize);
    p->prev->setMaximumSize(p->labelSize);
    p->play->setMaximumSize(p->labelSize);
    p->next->setMaximumSize(p->labelSize);

    p->layout->addWidget(p->stop, 0, Qt::AlignVCenter);
    p->layout->addWidget(p->prev, 0, Qt::AlignVCenter);
    p->layout->addWidget(p->play, 0, Qt::AlignVCenter);
    p->layout->addWidget(p->next, 0, Qt::AlignVCenter);

    setEnabled(false);
}

void PlayerControl::stateChanged(Player::PlayState state)
{
    switch(state) {
        case(Player::PlayState::Stopped):
            setEnabled(false);
            return p->play->setIcon(Core::Constants::Icons::Play);
        case(Player::PlayState::Playing):
            setEnabled(true);
            return p->play->setIcon(Core::Constants::Icons::Pause);
        case(Player::PlayState::Paused):
            return p->play->setIcon(Core::Constants::Icons::Play);
    }
}
} // namespace Core::Widgets
