/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include "fileopsdefs.h"

#include <core/plugins/coreplugin.h>
#include <core/plugins/plugin.h>
#include <gui/plugins/guiplugin.h>
#include <utils/id.h>

#include <memory>
#include <vector>

class QAction;

namespace Fooyin {
class AudioLoader;
class Command;
struct TrackSelection;

namespace FileOps {
class FileOpsPlugin : public QObject,
                      public Plugin,
                      public CorePlugin,
                      public GuiPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.fooyin.fooyin.plugin/1.0" FILE "fileops.json")
    Q_INTERFACES(Fooyin::Plugin Fooyin::CorePlugin Fooyin::GuiPlugin)

public:
    FileOpsPlugin();

    void initialise(const CorePluginContext& context) override;
    void initialise(const GuiPluginContext& context) override;

private:
    struct PresetAction
    {
        Operation operation;
        QString presetName;
        Id id;
        QAction* action;
        Command* command;
    };

    void setupMenu();
    void openDialog(const TrackSelection& selection, Operation operation, const QString& presetName = {});
    void refreshPresetActions();

    ActionManager* m_actionManager;
    std::shared_ptr<AudioLoader> m_audioLoader;
    MusicLibrary* m_library;
    LibraryManager* m_libraryManager;
    TrackSelectionController* m_trackSelectionController;
    SettingsManager* m_settings;
    std::vector<PresetAction> m_presetActions;
};
} // namespace FileOps
} // namespace Fooyin
