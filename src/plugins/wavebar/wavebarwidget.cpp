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

#include "wavebarconstants.h"
#include "waveformbuilder.h"
#include "waveseekbar.h"

#include <core/engine/enginecontroller.h>
#include <core/player/playercontroller.h>
#include <utils/settings/settingsdialogcontroller.h>
#include <utils/settings/settingsmanager.h>

#include <QActionGroup>
#include <QContextMenuEvent>
#include <QMenu>
#include <QVBoxLayout>

constexpr auto SeekDelta = 5000;

namespace Fooyin::WaveBar {
WaveBarWidget::WaveBarWidget(PlayerController* playerController, EngineController* engine, SettingsManager* settings,
                             QWidget* parent)
    : FyWidget{parent}
    , m_playerController{playerController}
    , m_settings{settings}
    , m_seekbar{new WaveSeekBar(settings, this)}
    , m_builder{engine->createDecoder(), settings}
{
    setMinimumSize(100, 20);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_seekbar);

    QObject::connect(m_seekbar, &WaveSeekBar::sliderMoved, m_playerController, &PlayerController::seek);
    QObject::connect(m_seekbar, &WaveSeekBar::seekForward, this, [this]() { m_playerController->seekForward(SeekDelta); });
    QObject::connect(m_seekbar, &WaveSeekBar::seekBackward, this, [this]() { m_playerController->seekBackward(SeekDelta); });
    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this, [this](const Track& track) {
        m_seekbar->setPosition(0);
        m_builder.generate(track);
    });
    QObject::connect(m_playerController, &PlayerController::positionChanged, m_seekbar, &WaveSeekBar::setPosition);
    QObject::connect(&m_builder, &WaveformBuilder::generatingWaveform, this, [this]() { m_seekbar->processData({}); });
    QObject::connect(&m_builder, &WaveformBuilder::waveformRescaled, m_seekbar, &WaveSeekBar::processData);

    if(m_playerController->currentTrack().isValid()) {
        m_builder.generate(m_playerController->currentTrack());
    }
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

    m_builder.rescale(m_seekbar->contentsRect().width());
}

void WaveBarWidget::contextMenuEvent(QContextMenuEvent* event)
{
    if(m_seekbar->isSeeking()) {
        m_seekbar->stopSeeking();
        return;
    }

    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* showCursor = new QAction(tr("Show Cursor"), menu);
    showCursor->setCheckable(true);
    showCursor->setChecked(m_settings->value<Settings::WaveBar::ShowCursor>());
    QObject::connect(showCursor, &QAction::triggered, this,
                     [this](bool checked) { m_settings->set<Settings::WaveBar::ShowCursor>(checked); });

    auto* modeMenu = new QMenu(tr("Mode"), menu);

    auto* minMaxMode = new QAction(tr("MinMax"), modeMenu);
    auto* rmsMode    = new QAction(tr("RMS"), modeMenu);

    minMaxMode->setCheckable(true);
    rmsMode->setCheckable(true);

    const auto currentMode = static_cast<WaveModes>(m_settings->value<Settings::WaveBar::Mode>());
    minMaxMode->setChecked(currentMode & WaveMode::MinMax);
    rmsMode->setChecked(currentMode & WaveMode::Rms);

    auto updateMode = [this](WaveMode mode, bool enable) {
        auto updatedMode = static_cast<WaveModes>(m_settings->value<Settings::WaveBar::Mode>());
        if(enable) {
            updatedMode |= mode;
        }
        else {
            updatedMode &= ~mode;
        }
        m_settings->set<Settings::WaveBar::Mode>(static_cast<int>(updatedMode));
    };

    QObject::connect(minMaxMode, &QAction::triggered, this,
                     [updateMode](bool checked) { updateMode(WaveMode::MinMax, checked); });
    QObject::connect(rmsMode, &QAction::triggered, this,
                     [updateMode](bool checked) { updateMode(WaveMode::Rms, checked); });

    modeMenu->addAction(minMaxMode);
    modeMenu->addAction(rmsMode);

    auto* downmixMenu  = new QMenu(tr("Downmix"), menu);
    auto* downmixGroup = new QActionGroup(downmixMenu);

    auto* downmixOff    = new QAction(tr("Off"), downmixGroup);
    auto* downmixStereo = new QAction(tr("Stereo"), downmixGroup);
    auto* downmixMono   = new QAction(tr("Mono"), downmixGroup);

    downmixOff->setCheckable(true);
    downmixStereo->setCheckable(true);
    downmixMono->setCheckable(true);

    const auto downmixOption = static_cast<DownmixOption>(m_settings->value<Settings::WaveBar::Downmix>());
    if(downmixOption == DownmixOption::Off) {
        downmixOff->setChecked(true);
    }
    else if(downmixOption == DownmixOption::Stereo) {
        downmixStereo->setChecked(true);
    }
    else {
        downmixMono->setChecked(true);
    }

    QObject::connect(downmixOff, &QAction::triggered, this,
                     [this]() { m_settings->set<Settings::WaveBar::Downmix>(0); });
    QObject::connect(downmixStereo, &QAction::triggered, this,
                     [this]() { m_settings->set<Settings::WaveBar::Downmix>(1); });
    QObject::connect(downmixMono, &QAction::triggered, this,
                     [this]() { m_settings->set<Settings::WaveBar::Downmix>(2); });

    downmixMenu->addAction(downmixOff);
    downmixMenu->addAction(downmixStereo);
    downmixMenu->addAction(downmixMono);

    auto* gotoSettings = new QAction(tr("Settings…"), menu);
    QObject::connect(gotoSettings, &QAction::triggered, this,
                     [this]() { m_settings->settingsDialog()->openAtPage(Constants::Page::WaveBarGeneral); });

    menu->addAction(showCursor);
    menu->addMenu(modeMenu);
    menu->addMenu(downmixMenu);
    menu->addSeparator();
    menu->addAction(gotoSettings);

    menu->popup(event->globalPos());
}
} // namespace Fooyin::WaveBar

#include "moc_wavebarwidget.cpp"
