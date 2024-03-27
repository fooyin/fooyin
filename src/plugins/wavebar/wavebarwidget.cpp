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
WaveBarWidget::WaveBarWidget(PlayerController* playerController, EngineController* engine, SettingsManager* settings,
                             QWidget* parent)
    : FyWidget{parent}
    , m_playerController{playerController}
    , m_engine{engine}
    , m_settings{settings}
    , m_seekbar{new WaveSeekBar(settings, this)}
    , m_builder{engine->createDecoder(), settings}
{
    setMinimumSize(100, 20);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(m_seekbar);

    m_builder.moveToThread(&m_builderThread);

    QObject::connect(m_seekbar, &WaveSeekBar::sliderMoved, m_playerController, &PlayerController::changePosition);
    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this, [this](const Track& track) {
        m_seekbar->setPosition(0);
        m_builder.stopThread();
        QMetaObject::invokeMethod(&m_builder, "rebuild", Q_ARG(const Track&, track));
    });
    QObject::connect(m_playerController, &PlayerController::positionChanged, m_seekbar, &WaveSeekBar::setPosition);
    QObject::connect(&m_builder, &WaveformBuilder::waveformBuilt, this, [this]() {
        m_builder.stopThread();
        QMetaObject::invokeMethod(&m_builder, "setWidth", Q_ARG(int, width()));
        QMetaObject::invokeMethod(&m_builder, &WaveformBuilder::rescale);
    });
    QObject::connect(&m_builder, &WaveformBuilder::waveformRescaled, m_seekbar, &WaveSeekBar::processData);

    m_builderThread.start();

    QMetaObject::invokeMethod(&m_builder, &Worker::initialiseThread);

    if(m_playerController->currentTrack().isValid()) {
        QMetaObject::invokeMethod(&m_builder, "rebuild", Q_ARG(const Track&, m_playerController->currentTrack()));
    }
}

WaveBarWidget::~WaveBarWidget()
{
    m_builder.stopThread();
    m_builderThread.quit();
    m_builderThread.wait();
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
    QMetaObject::invokeMethod(&m_builder, "setWidth", Q_ARG(int, width()));
    QMetaObject::invokeMethod(&m_builder, &WaveformBuilder::rescale);
}
} // namespace Fooyin::WaveBar

#include "moc_wavebarwidget.cpp"
