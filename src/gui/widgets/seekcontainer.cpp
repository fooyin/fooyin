/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include <gui/widgets/seekcontainer.h>

#include <core/player/playercontroller.h>
#include <utils/clickablelabel.h>
#include <utils/utils.h>

#include <QFontMetrics>
#include <QHBoxLayout>

namespace Fooyin {
struct SeekContainer::Private
{
    SeekContainer* self;

    PlayerController* playerController;

    QHBoxLayout* layout;
    ClickableLabel* elapsed;
    ClickableLabel* total;

    uint64_t max{0};
    bool elapsedTotal{false};

    Private(SeekContainer* self_, PlayerController* playerController_)
        : self{self_}
        , playerController{playerController_}
        , layout{new QHBoxLayout(self)}
        , elapsed{new ClickableLabel(Utils::msToString(0), self)}
        , total{new ClickableLabel(Utils::msToString(0), self)}
    {
        layout->setContentsMargins({});

        const QFontMetrics fm{self->fontMetrics()};
        elapsed->setMinimumWidth(fm.horizontalAdvance(Utils::msToString(0)));
        total->setMinimumWidth(fm.horizontalAdvance(QStringLiteral("-") + Utils::msToString(0)));

        layout->addWidget(elapsed, 0, Qt::AlignVCenter | Qt::AlignLeft);
        layout->addWidget(total, 0, Qt::AlignVCenter | Qt::AlignLeft);

        trackChanged(playerController->currentTrack());
    }

    void reset()
    {
        max = 0;
        updateLabels(max);
    }

    void trackChanged(const Track& track)
    {
        if(track.isValid()) {
            max = track.duration();
            updateLabels(0);
        }
    }

    void stateChanged(PlayState state)
    {
        switch(state) {
            case(PlayState::Paused):
                break;
            case(PlayState::Stopped):
                reset();
                break;
            case(PlayState::Playing): {
                if(max == 0) {
                    trackChanged(playerController->currentTrack());
                }
                break;
            }
        }
    }

    void updateLabels(uint64_t time) const
    {
        elapsed->setText(Utils::msToString(time));

        if(elapsedTotal) {
            const int remaining = static_cast<int>(max - time);
            total->setText(QStringLiteral("-") + Utils::msToString(remaining < 0 ? 0 : remaining));
        }
        else {
            total->setText(Utils::msToString(max));
        }
    }
};

SeekContainer::SeekContainer(PlayerController* playerController, QWidget* parent)
    : QWidget{parent}
    , p{std::make_unique<Private>(this, playerController)}
{
    QObject::connect(p->elapsed, &ClickableLabel::clicked, this, &SeekContainer::elapsedClicked);
    QObject::connect(p->total, &ClickableLabel::clicked, this, &SeekContainer::totalClicked);

    QObject::connect(p->playerController, &PlayerController::playStateChanged, this,
                     [this](PlayState state) { p->stateChanged(state); });
    QObject::connect(p->playerController, &PlayerController::currentTrackChanged, this,
                     [this](const Track& track) { p->trackChanged(track); });
    QObject::connect(p->playerController, &PlayerController::positionChanged, this,
                     [this](uint64_t pos) { p->updateLabels(pos); });

    QObject::connect(this, &SeekContainer::totalClicked, this, [this]() { setElapsedTotal(!elapsedTotal()); });
}

SeekContainer::~SeekContainer() = default;

void SeekContainer::insertWidget(int index, QWidget* widget)
{
    p->layout->insertWidget(index, widget, Qt::AlignVCenter);
}

bool SeekContainer::labelsEnabled() const
{
    return !p->elapsed->isHidden() && !p->total->isHidden();
}

bool SeekContainer::elapsedTotal() const
{
    return p->elapsedTotal;
}

void SeekContainer::setLabelsEnabled(bool enabled)
{
    p->elapsed->setHidden(!enabled);
    p->total->setHidden(!enabled);
}

void SeekContainer::setElapsedTotal(bool enabled)
{
    p->elapsedTotal = enabled;
    if(!p->elapsedTotal) {
        p->total->setText(Utils::msToString(p->max));
    }
}
} // namespace Fooyin
