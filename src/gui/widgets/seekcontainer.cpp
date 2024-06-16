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
    SeekContainer* m_self;

    PlayerController* m_playerController;

    QHBoxLayout* m_layout;
    ClickableLabel* m_elapsed;
    ClickableLabel* m_total;

    uint64_t m_max{0};
    bool m_elapsedTotal{false};

    Private(SeekContainer* self, PlayerController* playerController)
        : m_self{self}
        , m_playerController{playerController}
        , m_layout{new QHBoxLayout(m_self)}
        , m_elapsed{new ClickableLabel(Utils::msToString(0), m_self)}
        , m_total{new ClickableLabel(Utils::msToString(0), m_self)}
    {
        m_layout->setContentsMargins({});

        const QFontMetrics fm{m_self->fontMetrics()};
        m_elapsed->setMinimumWidth(fm.horizontalAdvance(Utils::msToString(0)));
        m_total->setMinimumWidth(fm.horizontalAdvance(QStringLiteral("-") + Utils::msToString(0)));

        m_layout->addWidget(m_elapsed, 0, Qt::AlignVCenter | Qt::AlignLeft);
        m_layout->addWidget(m_total, 0, Qt::AlignVCenter | Qt::AlignLeft);

        trackChanged(m_playerController->currentTrack());
    }

    void reset()
    {
        m_max = 0;
        updateLabels(m_max);
    }

    void trackChanged(const Track& track)
    {
        if(track.isValid()) {
            m_max = track.duration();
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
                if(m_max == 0) {
                    trackChanged(m_playerController->currentTrack());
                }
                break;
            }
        }
    }

    void updateLabels(uint64_t time) const
    {
        m_elapsed->setText(Utils::msToString(time));

        if(m_elapsedTotal) {
            const int remaining = static_cast<int>(m_max - time);
            m_total->setText(QStringLiteral("-") + Utils::msToString(remaining < 0 ? 0 : remaining));
        }
        else {
            m_total->setText(Utils::msToString(m_max));
        }
    }
};

SeekContainer::SeekContainer(PlayerController* playerController, QWidget* parent)
    : QWidget{parent}
    , p{std::make_unique<Private>(this, playerController)}
{
    QObject::connect(p->m_elapsed, &ClickableLabel::clicked, this, &SeekContainer::elapsedClicked);
    QObject::connect(p->m_total, &ClickableLabel::clicked, this, &SeekContainer::totalClicked);

    QObject::connect(p->m_playerController, &PlayerController::playStateChanged, this,
                     [this](PlayState state) { p->stateChanged(state); });
    QObject::connect(p->m_playerController, &PlayerController::currentTrackChanged, this,
                     [this](const Track& track) { p->trackChanged(track); });
    QObject::connect(p->m_playerController, &PlayerController::positionChanged, this,
                     [this](uint64_t pos) { p->updateLabels(pos); });

    QObject::connect(this, &SeekContainer::totalClicked, this, [this]() { setElapsedTotal(!elapsedTotal()); });
}

SeekContainer::~SeekContainer() = default;

void SeekContainer::insertWidget(int index, QWidget* widget)
{
    p->m_layout->insertWidget(index, widget, Qt::AlignVCenter);
}

bool SeekContainer::labelsEnabled() const
{
    return !p->m_elapsed->isHidden() && !p->m_total->isHidden();
}

bool SeekContainer::elapsedTotal() const
{
    return p->m_elapsedTotal;
}

void SeekContainer::setLabelsEnabled(bool enabled)
{
    p->m_elapsed->setHidden(!enabled);
    p->m_total->setHidden(!enabled);
}

void SeekContainer::setElapsedTotal(bool enabled)
{
    p->m_elapsedTotal = enabled;
    if(!p->m_elapsedTotal) {
        p->m_total->setText(Utils::msToString(p->m_max));
    }
}
} // namespace Fooyin
