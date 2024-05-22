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
#include <gui/widgets/seekcontainer.h>
#include <utils/settings/settingsdialogcontroller.h>
#include <utils/settings/settingsmanager.h>

#include <QActionGroup>
#include <QContextMenuEvent>
#include <QMenu>
#include <QVBoxLayout>

// TODO: Make setting
constexpr auto SeekDelta = 5000;

namespace Fooyin::WaveBar {
WaveBarWidget::WaveBarWidget(WaveformBuilder* builder, PlayerController* playerController, SettingsManager* settings,
                             QWidget* parent)
    : FyWidget{parent}
    , m_playerController{playerController}
    , m_settings{settings}
    , m_container{new SeekContainer(m_playerController, this)}
    , m_seekbar{new WaveSeekBar(settings, this)}
    , m_builder{builder}
{
    setMinimumSize(100, 20);
    resize(100, 100);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_container);
    m_container->insertWidget(1, m_seekbar);

    QObject::connect(m_seekbar, &WaveSeekBar::sliderMoved, this, &WaveBarWidget::seek);
    QObject::connect(m_seekbar, &WaveSeekBar::seekForward, this, &WaveBarWidget::seekForward);
    QObject::connect(m_seekbar, &WaveSeekBar::seekBackward, this, &WaveBarWidget::seekBackward);

    QObject::connect(m_builder, &WaveformBuilder::generatingWaveform, this, [this]() { m_seekbar->processData({}); });
    QObject::connect(m_builder, &WaveformBuilder::waveformRescaled, m_seekbar, &WaveSeekBar::processData);

    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this, &WaveBarWidget::changeTrack);
    QObject::connect(playerController, &PlayerController::positionChanged, this, &WaveBarWidget::changePosition);
    QObject::connect(this, &WaveBarWidget::seek, playerController, &PlayerController::seek);
    QObject::connect(this, &WaveBarWidget::seekForward, playerController,
                     [this]() { m_playerController->seekForward(SeekDelta); });
    QObject::connect(this, &WaveBarWidget::seekBackward, playerController,
                     [this]() { m_playerController->seekBackward(SeekDelta); });

    QObject::connect(m_container, &SeekContainer::totalClicked, this, [this]() {
        rescaleWaveform(); // Switching to elapsed total may change the width
    });
}

QString WaveBarWidget::name() const
{
    return QStringLiteral("Waveform Seekbar");
}

QString WaveBarWidget::layoutName() const
{
    return QStringLiteral("WaveBar");
}

void WaveBarWidget::saveLayoutData(QJsonObject& layout)
{
    layout[QStringLiteral("ShowLabels")]   = m_container->labelsEnabled();
    layout[QStringLiteral("ElapsedTotal")] = m_container->elapsedTotal();
}

void WaveBarWidget::loadLayoutData(const QJsonObject& layout)
{
    if(layout.contains(QStringLiteral("ShowLabels"))) {
        const bool showLabels = layout.value(QStringLiteral("ShowLabels")).toBool();
        m_container->setLabelsEnabled(showLabels);
    }
    if(layout.contains(QStringLiteral("ElapsedTotal"))) {
        const bool elapsedTotal = layout.value(QStringLiteral("ElapsedTotal")).toBool();
        m_container->setElapsedTotal(elapsedTotal);
    }
}

void WaveBarWidget::changeTrack(const Track& track)
{
    m_seekbar->setPosition(0);
    m_builder->generateAndScale(track);
}

void WaveBarWidget::changePosition(uint64_t pos)
{
    m_seekbar->setPosition(pos);
}

void WaveBarWidget::showEvent(QShowEvent* event)
{
    FyWidget::showEvent(event);

    rescaleWaveform();
}

void WaveBarWidget::resizeEvent(QResizeEvent* event)
{
    FyWidget::resizeEvent(event);

    rescaleWaveform();
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

    auto* showLabels = new QAction(tr("Show Labels"), this);
    showLabels->setCheckable(true);
    showLabels->setChecked(m_container->labelsEnabled());
    QObject::connect(showLabels, &QAction::triggered, this, [this](bool checked) {
        m_container->setLabelsEnabled(checked);
        // Width won't be updated immediately, so use a queued connection
        QMetaObject::invokeMethod(
            m_container, [this]() { rescaleWaveform(); }, Qt::QueuedConnection);
    });

    auto* showElapsed = new QAction(tr("Show Elapsed Total"), this);
    showElapsed->setCheckable(true);
    showElapsed->setChecked(m_container->elapsedTotal());
    QObject::connect(showElapsed, &QAction::triggered, this, [this](bool checked) {
        m_container->setElapsedTotal(checked);
        rescaleWaveform();
    });

    auto* modeMenu = new QMenu(tr("Mode"), menu);

    auto* minMaxMode  = new QAction(tr("Min/Max"), modeMenu);
    auto* rmsMode     = new QAction(tr("RMS"), modeMenu);
    auto* silenceMode = new QAction(tr("Silence"), modeMenu);

    minMaxMode->setCheckable(true);
    rmsMode->setCheckable(true);
    silenceMode->setCheckable(true);

    const auto currentMode = static_cast<WaveModes>(m_settings->value<Settings::WaveBar::Mode>());
    minMaxMode->setChecked(currentMode & WaveMode::MinMax);
    rmsMode->setChecked(currentMode & WaveMode::Rms);
    silenceMode->setChecked(currentMode & WaveMode::Silence);

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
    QObject::connect(silenceMode, &QAction::triggered, this,
                     [updateMode](bool checked) { updateMode(WaveMode::Silence, checked); });

    modeMenu->addAction(minMaxMode);
    modeMenu->addAction(rmsMode);
    modeMenu->addAction(silenceMode);

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
    menu->addAction(showLabels);
    menu->addAction(showElapsed);
    menu->addSeparator();
    menu->addMenu(modeMenu);
    menu->addMenu(downmixMenu);
    menu->addSeparator();
    menu->addAction(gotoSettings);

    menu->popup(event->globalPos());
}

void WaveBarWidget::rescaleWaveform()
{
    m_builder->rescale(m_seekbar->contentsRect().width());
}
} // namespace Fooyin::WaveBar

#include "moc_wavebarwidget.cpp"
