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

#pragma once

#include <core/plugins/coreplugin.h>
#include <core/plugins/plugin.h>
#include <core/track.h>
#include <gui/plugins/guiplugin.h>
#include <utils/database/dbconnectionpool.h>

#include <QPointer>

#include <vector>

namespace Fooyin {
class FyWidget;

namespace WaveBar {
class WaveBarSettings;
class WaveformBuilder;
class WaveBarWidget;

class WaveBarPlugin : public QObject,
                      public Plugin,
                      public CorePlugin,
                      public GuiPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.fooyin.fooyin.plugin/1.0" FILE "wavebar.json")
    Q_INTERFACES(Fooyin::Plugin Fooyin::CorePlugin Fooyin::GuiPlugin)

public:
    WaveBarPlugin();
    ~WaveBarPlugin() override;

    void initialise(const CorePluginContext& context) override;
    void initialise(const GuiPluginContext& context) override;

private:
    FyWidget* createWavebar();
    void registerWaveBar(WaveBarWidget* widget);
    void pruneWaveBars();
    void refreshWaveBars(const Track& track, bool update = false);

    void regenerateSelection(bool onlyMissing = false);
    void removeTrack(const Track& track);
    void removeTracks(const TrackList& tracks);
    void removeSelection();
    void clearCache();

    ActionManager* m_actionManager;
    PlayerController* m_playerController;
    EngineController* m_engine;
    std::shared_ptr<AudioLoader> m_audioLoader;
    TrackSelectionController* m_trackSelection;
    WidgetProvider* m_widgetProvider;
    SettingsManager* m_settings;

    Track m_playingTrack;
    DbConnectionPoolPtr m_dbPool;
    std::vector<QPointer<WaveBarWidget>> m_waveBars;

    std::unique_ptr<WaveBarSettings> m_waveBarSettings;
};
} // namespace WaveBar
} // namespace Fooyin
