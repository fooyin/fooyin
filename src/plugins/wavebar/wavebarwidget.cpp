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

#include "wavebarwidget.h"

#include "waveformbuilder.h"
#include "waveseekbar.h"

#include <core/engine/enginecontroller.h>
#include <core/player/playercontroller.h>

#include <QThread>
#include <QVBoxLayout>

namespace Fooyin::WaveBar {
struct WaveBarWidget::Private
{
    WaveBarWidget* self;

    PlayerController* playerController;
    EngineController* engine;
    SettingsManager* settings;

    WaveSeekBar* seekbar;
    WaveformBuilder builder;
    QThread builderThread;

    Private(WaveBarWidget* self_, PlayerController* playerController_, EngineController* engine_,
            SettingsManager* settings_)
        : self{self_}
        , playerController{playerController_}
        , engine{engine_}
        , settings{settings_}
        , seekbar{new WaveSeekBar(self)}
        , builder{engine->createDecoder()}
    {
        auto* layout = new QVBoxLayout(self);
        layout->setContentsMargins(0, 0, 0, 0);

        layout->addWidget(seekbar);

        builder.moveToThread(&builderThread);

        QObject::connect(seekbar, &WaveSeekBar::sliderMoved, self,
                         [this](int pos) { playerController->changePosition(pos); });
        QObject::connect(playerController, &PlayerController::currentTrackChanged, self, [this](const Track& track) {
            seekbar->setPosition(0);
            QMetaObject::invokeMethod(&builder, "rebuild", Q_ARG(const Track&, track));
        });
        QObject::connect(playerController, &PlayerController::positionChanged, self,
                         [this](uint64_t pos) { seekbar->setPosition(pos); });
        QObject::connect(&builder, &WaveformBuilder::waveformBuilt, self, [this]() {
            QMetaObject::invokeMethod(&builder, "setWidth", Q_ARG(int, self->width()));
            QMetaObject::invokeMethod(&builder, &WaveformBuilder::rescale);
        });
        QObject::connect(&builder, &WaveformBuilder::waveformRescaled, seekbar, &WaveSeekBar::processData);

        builderThread.start();

        QMetaObject::invokeMethod(&builder, &Worker::initialiseThread);
    }
};

WaveBarWidget::WaveBarWidget(PlayerController* playerController, EngineController* engine, SettingsManager* settings,
                             QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<Private>(this, playerController, engine, settings)}
{
    setMinimumSize(50, 20);
}

WaveBarWidget::~WaveBarWidget()
{
    p->builderThread.quit();
    p->builderThread.wait();
}

QString WaveBarWidget::name() const
{
    return QStringLiteral("Waveform Seekbar");
}

QString WaveBarWidget::layoutName() const
{
    return QStringLiteral("WaveBar");
}

void WaveBarWidget::resizeEvent(QResizeEvent* event)
{
    FyWidget::resizeEvent(event);

    // TODO: Group resize calls to avoid rescaling waveform too frequently
    QMetaObject::invokeMethod(&p->builder, "setWidth", Q_ARG(int, width()));
    QMetaObject::invokeMethod(&p->builder, &WaveformBuilder::rescale);
}
} // namespace Fooyin::WaveBar

#include "moc_wavebarwidget.cpp"
