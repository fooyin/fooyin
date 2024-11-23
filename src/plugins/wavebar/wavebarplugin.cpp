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

#include "wavebarplugin.h"

#include "settings/wavebarguisettingspage.h"
#include "settings/wavebarsettings.h"
#include "settings/wavebarsettingspage.h"
#include "wavebarconstants.h"
#include "wavebarwidget.h"
#include "waveformbuilder.h"

#include <core/engine/enginecontroller.h>
#include <core/player/playercontroller.h>
#include <gui/guiconstants.h>
#include <gui/trackselectioncontroller.h>
#include <gui/widgetprovider.h>
#include <gui/widgets/elapsedprogressdialog.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/async.h>
#include <utils/utils.h>

#include <QMainWindow>
#include <QMenu>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;

namespace {
Fooyin::DbConnection::DbParams dbConnectionParams()
{
    Fooyin::DbConnection::DbParams params;
    params.type           = u"QSQLITE"_s;
    params.connectOptions = u"QSQLITE_OPEN_URI"_s;
    params.filePath       = Fooyin::WaveBar::cachePath();

    return params;
}
} // namespace

namespace Fooyin::WaveBar {
WaveBarPlugin::WaveBarPlugin()
    : m_dbPool{DbConnectionPool::create(dbConnectionParams(), u"wavebar"_s)}
{ }

WaveBarPlugin::~WaveBarPlugin()
{
    m_waveBuilder.reset();
}

void WaveBarPlugin::initialise(const CorePluginContext& context)
{
    m_playerController = context.playerController;
    m_engine           = context.engine;
    m_audioLoader      = context.audioLoader;
    m_settings         = context.settingsManager;

    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this,
                     [this](const Track& track) { m_playingTrack = track; });
    QObject::connect(m_engine, &EngineController::trackChanged, this, [this](const Track& track) {
        if(m_playingTrack.id() == track.id()) {
            removeTrack(m_playingTrack);
        }
    });
}

void WaveBarPlugin::initialise(const GuiPluginContext& context)
{
    m_actionManager  = context.actionManager;
    m_trackSelection = context.trackSelection;
    m_widgetProvider = context.widgetProvider;

    m_waveBarSettings        = std::make_unique<WaveBarSettings>(m_settings);
    m_waveBarSettingsPage    = std::make_unique<WaveBarSettingsPage>(m_settings);
    m_waveBarGuiSettingsPage = std::make_unique<WaveBarGuiSettingsPage>(m_settings);

    QObject::connect(m_waveBarSettingsPage.get(), &WaveBarSettingsPage::clearCache, this, &WaveBarPlugin::clearCache);

    m_widgetProvider->registerWidget(u"WaveBar"_s, [this]() { return createWavebar(); }, tr("Waveform Seekbar"));
    m_widgetProvider->setSubMenus(u"WaveBar"_s, {tr("Controls")});

    auto* selectionMenu = m_actionManager->actionContainer(::Fooyin::Constants::Menus::Context::TrackSelection);
    auto* utilitiesMenu = m_actionManager->createMenu(::Fooyin::Constants::Menus::Context::Utilities);
    utilitiesMenu->menu()->setTitle(tr("Utilities"));
    selectionMenu->addMenu(utilitiesMenu);

    auto* window = Utils::getMainWindow();

    auto* regenerate = new QAction(tr("Regenerate waveform data"), window);
    regenerate->setStatusTip(tr("Regenerate waveform data for the selected tracks"));
    QObject::connect(regenerate, &QAction::triggered, this, [this]() { regenerateSelection(); });
    utilitiesMenu->addAction(regenerate);

    auto* regenerateMissing = new QAction(tr("Generate missing waveform data"), window);
    regenerateMissing->setStatusTip(tr("Generate waveform data for the selected tracks if missing"));
    QObject::connect(regenerateMissing, &QAction::triggered, this, [this]() { regenerateSelection(true); });
    utilitiesMenu->addAction(regenerateMissing);

    auto* removeData = new QAction(tr("Remove waveform data"), window);
    removeData->setStatusTip(tr("Remove any existing waveform data for the selected tracks"));
    QObject::connect(removeData, &QAction::triggered, this, &WaveBarPlugin::removeSelection);
    utilitiesMenu->addAction(removeData);
}

FyWidget* WaveBarPlugin::createWavebar()
{
    if(!m_waveBuilder) {
        m_waveBuilder = std::make_unique<WaveformBuilder>(m_audioLoader, m_dbPool, m_settings);
    }

    auto* wavebar = new WaveBarWidget(m_waveBuilder.get(), m_playerController, m_settings);

    const Track currTrack = m_playerController->currentTrack();
    if(currTrack.isValid()) {
        m_waveBuilder->generateAndScale(currTrack);
    }

    return wavebar;
}

void WaveBarPlugin::regenerateSelection(bool onlyMissing) const
{
    auto selectedTracks = m_trackSelection->selectedTracks();
    if(selectedTracks.empty()) {
        return;
    }

    const Track currentTrack = m_playerController->currentTrack();
    auto currIt              = std::ranges::find(selectedTracks, currentTrack);
    if(m_waveBuilder && currIt != selectedTracks.cend()) {
        selectedTracks.erase(currIt);
        m_waveBuilder->generateAndScale(currentTrack, !onlyMissing);
    }

    if(selectedTracks.empty()) {
        return;
    }

    const auto total = static_cast<int>(selectedTracks.size());
    auto* dialog
        = new ElapsedProgressDialog(tr("Generating waveform data…"), tr("Abort"), 0, total, Utils::getMainWindow());
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setMinimumDuration(500ms);

    // Deleted on close of dialog
    auto* builder = new WaveformBuilder(m_audioLoader, m_dbPool, m_settings, dialog);

    QObject::connect(builder, &WaveformBuilder::waveformGenerated, dialog, [dialog, total](const Track& track) {
        dialog->setValue(dialog->value() + 1);
        if(track.isValid()) {
            dialog->setText(track.prettyFilepath());
        }
        if(dialog->value() >= total) {
            dialog->close();
        }
    });

    for(const Track& track : selectedTracks) {
        builder->generate(track, !onlyMissing);
    }
}

void WaveBarPlugin::removeTrack(const Track& track)
{
    removeTracks({track});
}

void WaveBarPlugin::removeTracks(const TrackList& tracks)
{
    if(tracks.empty()) {
        return;
    }

    Utils::asyncExec([this, tracks]() {
        QStringList keys;
        for(const Track& track : tracks) {
            keys.emplace_back(WaveBarDatabase::cacheKey(track));
        }

        const DbConnectionHandler dbHandler{m_dbPool};
        WaveBarDatabase waveDb;
        waveDb.initialise(DbConnectionProvider{m_dbPool});
        waveDb.initialiseDatabase();

        if(!waveDb.removeFromCache(keys)) {
            qCWarning(WAVEBAR) << "Unable to remove waveform data";
        }
    });
}

void WaveBarPlugin::removeSelection()
{
    removeTracks(m_trackSelection->selectedTracks());
}

void WaveBarPlugin::clearCache() const
{
    const DbConnectionHandler handler{m_dbPool};
    WaveBarDatabase waveDb;
    waveDb.initialise(DbConnectionProvider{m_dbPool});

    if(!waveDb.clearCache()) {
        qCWarning(WAVEBAR) << "Unable to clear waveform cache";
    }
}
} // namespace Fooyin::WaveBar

#include "moc_wavebarplugin.cpp"
