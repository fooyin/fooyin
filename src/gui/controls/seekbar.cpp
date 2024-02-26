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

#include "seekbar.h"

#include "internalguisettings.h"

#include <core/player/playermanager.h>
#include <core/track.h>
#include <utils/clickablelabel.h>
#include <utils/settings/settingsmanager.h>
#include <utils/slider.h>
#include <utils/utils.h>

#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QMenu>
#include <QSlider>

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
    {
        toggleElapsedTotal(settings->value<Settings::Gui::Internal::SeekBarElapsedTotal>());
        toggleLabels(settings->value<Settings::Gui::Internal::SeekBarLabels>());

        settings->subscribe<Settings::Gui::Internal::SeekBarLabels>(self,
                                                                    [this](bool enabled) { toggleLabels(enabled); });
        settings->subscribe<Settings::Gui::Internal::SeekBarElapsedTotal>(
            self, [this](bool enabled) { toggleElapsedTotal(enabled); });
    }

    void reset()
    {
        elapsed->setText(QStringLiteral("00:00"));
        total->setText(elapsedTotal ? QStringLiteral("-00:00") : QStringLiteral("00:00"));
        slider->setValue(0);
        max = 0;
    }

    void trackChanged(const Track& track)
    {
        reset();

        if(track.isValid()) {
            max = track.duration();
            slider->setMaximum(static_cast<int>(max));
            const QString totalText = elapsedTotal ? QStringLiteral("-") : QStringLiteral("") + Utils::msToString(max);
            total->setText(totalText);
        }
    }

    void setCurrentPosition(uint64_t pos) const
    {
        if(!slider->isSliderDown()) {
            slider->setValue(static_cast<int>(pos));
            updateLabels(pos);
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
                slider->setEnabled(false);
                break;
            }
            case(PlayState::Playing): {
                slider->setEnabled(true);
                break;
            }
            case(PlayState::Paused): {
                return;
            }
        }
    }

    void toggleLabels(bool enabled) const
    {
        elapsed->setHidden(!enabled);
        total->setHidden(!enabled);
    }

    void toggleElapsedTotal(bool enabled)
    {
        elapsedTotal = enabled;

        if(!elapsedTotal) {
            total->setText(Utils::msToString(max));
        }
    }

    void sliderDropped() const
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

    p->slider->setEnabled(p->playerManager->currentTrack().isValid());

    QObject::connect(p->total, &ClickableLabel::clicked, this,
                     [this]() { p->settings->set<Settings::Gui::Internal::SeekBarElapsedTotal>(!p->elapsedTotal); });
    QObject::connect(p->slider, &Slider::sliderReleased, this, [this]() { p->sliderDropped(); });

    QObject::connect(p->playerManager, &PlayerManager::playStateChanged, this,
                     [this](PlayState state) { p->stateChanged(state); });
    QObject::connect(p->playerManager, &PlayerManager::currentTrackChanged, this,
                     [this](const Track& track) { p->trackChanged(track); });
    QObject::connect(p->playerManager, &PlayerManager::positionChanged, this,
                     [this](uint64_t pos) { p->setCurrentPosition(pos); });
    QObject::connect(p->playerManager, &PlayerManager::positionMoved, this,
                     [this](uint64_t pos) { p->setCurrentPosition(pos); });

    p->trackChanged(p->playerManager->currentTrack());
}

Fooyin::SeekBar::~SeekBar() = default;

QString SeekBar::name() const
{
    return QStringLiteral("SeekBar");
}

void SeekBar::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* showLabels = new QAction(tr("Show Labels"), this);
    showLabels->setCheckable(true);
    showLabels->setChecked(p->settings->value<Settings::Gui::Internal::SeekBarLabels>());
    QObject::connect(showLabels, &QAction::triggered, this,
                     [this](bool checked) { p->settings->set<Settings::Gui::Internal::SeekBarLabels>(checked); });
    menu->addAction(showLabels);

    auto* showElapsed = new QAction(tr("Show Elapsed Total"), this);
    showElapsed->setCheckable(true);
    showElapsed->setChecked(p->settings->value<Settings::Gui::Internal::SeekBarElapsedTotal>());
    QObject::connect(showElapsed, &QAction::triggered, this,
                     [this](bool checked) { p->settings->set<Settings::Gui::Internal::SeekBarElapsedTotal>(checked); });
    menu->addAction(showElapsed);

    menu->popup(event->globalPos());
}
} // namespace Fooyin

#include "moc_seekbar.cpp"
