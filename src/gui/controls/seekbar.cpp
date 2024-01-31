/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

#include "seekbar.h"

#include "internalguisettings.h"

#include <core/player/playermanager.h>
#include <core/track.h>
#include <utils/clickablelabel.h>
#include <utils/settings/settingsmanager.h>
#include <utils/slider.h>
#include <utils/utils.h>

#include <QHBoxLayout>
#include <QSlider>

using namespace Qt::Literals::StringLiterals;

namespace Fooyin {
struct SeekBar::Private
{
    SeekBar* self;

    PlayerManager* playerManager;
    SettingsManager* settings;

    Slider* slider;
    ClickableLabel* elapsed;
    ClickableLabel* total;
    uint64_t max{0};
    bool elapsedTotal;

    Private(SeekBar* self_, PlayerManager* playerManager_, SettingsManager* settings_)
        : self{self_}
        , playerManager{playerManager_}
        , settings{settings_}
        , slider{new Slider(Qt::Horizontal, self)}
        , elapsed{new ClickableLabel(self)}
        , total{new ClickableLabel(self)}
        , elapsedTotal{settings->value<Settings::Gui::Internal::ElapsedTotal>()}
    { }

    void reset()
    {
        elapsed->setText(u"00:00"_s);
        total->setText(elapsedTotal ? u"-00:00"_s : u"00:00"_s);
        slider->setValue(0);
        max = 0;
    }

    void trackChanged(const Track& track)
    {
        reset();

        if(track.isValid()) {
            max = track.duration();
            slider->setMaximum(static_cast<int>(max));
            const QString totalText = elapsedTotal ? u"-"_s : u""_s + Utils::msToString(max);
            total->setText(totalText);
        }
    }

    void setCurrentPosition(uint64_t pos)
    {
        if(!slider->isSliderDown()) {
            slider->setValue(static_cast<int>(pos));
            updateLabels(pos);
        }
    }

    void updateLabels(uint64_t time)
    {
        elapsed->setText(Utils::msToString(time));

        if(elapsedTotal) {
            const int remaining = static_cast<int>(max - time);
            total->setText("-" + Utils::msToString(remaining < 0 ? 0 : remaining));
        }
        else {
            if(!slider->isEnabled()) {
                total->setText(Utils::msToString(max));
            }
        }
    }

    void stateChanged(PlayState state)
    {
        switch(state) {
            case(PlayState::Stopped): {
                reset();
                self->setEnabled(false);
                break;
            }
            case(PlayState::Playing): {
                self->setEnabled(true);
                break;
            }
            case(PlayState::Paused): {
                return;
            }
        }
    }

    void toggleRemaining()
    {
        if(elapsedTotal) {
            settings->set<Settings::Gui::Internal::ElapsedTotal>(false);
            total->setText(Utils::msToString(max));
        }
        else {
            settings->set<Settings::Gui::Internal::ElapsedTotal>(true);
        }
        elapsedTotal = !elapsedTotal;
    }

    void sliderDropped()
    {
        const auto pos = static_cast<uint64_t>(slider->value());
        playerManager->changePosition(pos);
    }
};

SeekBar::SeekBar(PlayerManager* playerManager, SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<Private>(this, playerManager, settings)}
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 0, 10, 0);

    layout->addWidget(p->elapsed, 0, Qt::AlignVCenter | Qt::AlignLeft);
    layout->addWidget(p->slider, 0, Qt::AlignVCenter);
    layout->addWidget(p->total, 0, Qt::AlignVCenter | Qt::AlignLeft);

    setEnabled(p->playerManager->currentTrack().isValid());

    QObject::connect(p->total, &ClickableLabel::clicked, this, [this]() { p->toggleRemaining(); });
    QObject::connect(p->slider, &Slider::sliderReleased, this, [this]() { p->sliderDropped(); });

    QObject::connect(p->playerManager, &PlayerManager::playStateChanged, this,
                     [this](PlayState state) { p->stateChanged(state); });
    QObject::connect(p->playerManager, &PlayerManager::currentTrackChanged, this,
                     [this](const Track& track) { p->trackChanged(track); });
    QObject::connect(p->playerManager, &PlayerManager::positionChanged, this,
                     [this](uint64_t pos) { p->setCurrentPosition(pos); });

    p->trackChanged(p->playerManager->currentTrack());
}

Fooyin::SeekBar::~SeekBar() = default;

QString SeekBar::name() const
{
    return u"SeekBar"_s;
}
} // namespace Fooyin

#include "moc_seekbar.cpp"
