/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "runservices.h"

#include <core/plugins/coreplugin.h>
#include <core/plugins/plugin.h>
#include <core/track.h>
#include <gui/plugins/guiplugin.h>
#include <utils/id.h>

#include <QPointer>

#include <unordered_map>

class QAction;
class QMenu;

namespace Fooyin {
class ActionManager;
class SettingsManager;
class TrackSelectionController;

namespace RunServices {
class RunServicesPage;

class RunServicesPlugin : public QObject,
                          public Plugin,
                          public CorePlugin,
                          public GuiPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.fooyin.fooyin.plugin/1.0" FILE "runservices.json")
    Q_INTERFACES(Fooyin::Plugin Fooyin::CorePlugin Fooyin::GuiPlugin)

public:
    void initialise(const CorePluginContext& context) override;
    void initialise(const GuiPluginContext& context) override;

private:
    void registerRunServiceActions();
    void renderRunServicesMenu(QMenu* menu, const TrackList& tracks);
    void runServiceAction(const Id& id);
    void runService(const RunService& service, const TrackList& tracks);

    [[nodiscard]] QAction* registeredAction(const Id& id) const;
    void syncRegisteredAction(const Id& id, const QString& text, bool active);

    ActionManager* m_actionManager;
    SettingsManager* m_settings;
    TrackSelectionController* m_trackSelection;

    RunServicesPage* m_page;
    std::unordered_map<Id, QPointer<QAction>, Id::IdHash> m_runServiceActions;
};
} // namespace RunServices
} // namespace Fooyin
