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

#include "runservicesplugin.h"

#include "runserviceexecutor.h"
#include "runservices.h"
#include "runservicesconstants.h"
#include "runservicespage.h"

#include <gui/guiconstants.h>
#include <gui/trackselectioncontroller.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/settings/settingsmanager.h>

#include <QAction>
#include <QLoggingCategory>
#include <QMenu>

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(RUN_SERVICES, "fy.runservices")

namespace Fooyin::RunServices {
namespace {
Id actionId(const RunService& service)
{
    return Id{u"RunServices.%1"_s.arg(service.id)};
}
} // namespace

void RunServicesPlugin::initialise(const CorePluginContext& context)
{
    m_settings = context.settingsManager;

    m_settings->createSetting<Settings::RunServices::Services>(defaultRunServicesJson(), u"RunServices/Services"_s);

    const auto services = runServices(*m_settings);
    if(!services.empty()) {
        setRunServices(*m_settings, services);
    }
}

void RunServicesPlugin::initialise(const GuiPluginContext& context)
{
    m_actionManager  = context.actionManager;
    m_trackSelection = context.trackSelection;

    m_page = new RunServicesPage(m_settings, this);

    m_trackSelection->registerTrackContextDynamicSubmenu(
        this, TrackContextMenuArea::Track, Fooyin::Constants::Menus::Context::TrackSelection,
        Constants::Menus::RunServices, tr("Run"),
        [this](QMenu* menu, const TrackSelection& selection) { renderRunServicesMenu(menu, selection.tracks); },
        Fooyin::Constants::Menus::Context::TrackFinalSeparator);

    registerRunServiceActions();

    m_settings->subscribe<Settings::RunServices::Services>(this, &RunServicesPlugin::registerRunServiceActions);
}

void RunServicesPlugin::registerRunServiceActions()
{
    IdSet activeIds;

    const auto services = runServices(*m_settings);
    for(const RunService& service : services) {
        const Id id = actionId(service);
        activeIds.emplace(id);

        QAction* action = registeredAction(id);
        if(action) {
            syncRegisteredAction(id, service.name, true);
            if(Command* command = m_actionManager->command(id)) {
                command->setCategories({tr("Tracks"), tr("Run Services")});
            }
            continue;
        }

        action = new QAction(service.name, this);
        QObject::connect(action, &QAction::triggered, this, [this, id] { runServiceAction(id); });

        Command* command = m_actionManager->registerAction(action, id);
        //: %1 is the name of a registered run service
        command->setDescription(tr("Run %1").arg(service.name));
        command->setCategories({tr("Tracks"), tr("Run Services")});
        command->action()->setShortcutVisibleInContextMenu(true);
        m_runServiceActions[id] = action;
    }

    for(const auto& [id, action] : m_runServiceActions) {
        if(!activeIds.contains(id)) {
            syncRegisteredAction(id, action ? action->text() : QString{}, false);
        }
    }

    Q_EMIT m_actionManager->commandsChanged();
}

void RunServicesPlugin::renderRunServicesMenu(QMenu* menu, const TrackList& tracks)
{
    if(tracks.empty()) {
        return;
    }

    registerRunServiceActions();

    const auto services = runServices(*m_settings);
    for(const RunService& service : services) {
        if(!service.enabled) {
            continue;
        }

        const auto* command = m_actionManager->command(actionId(service));
        if(!command) {
            continue;
        }

        auto* action = command->action();
        action->setText(evaluateRunServiceLabel(service, tracks.front()));
        //: %1 is the name of a registered run service
        action->setStatusTip(tr("Run %1").arg(service.name));
        action->setEnabled(true);
        menu->addAction(action);
    }
}

void RunServicesPlugin::runServiceAction(const Id& id)
{
    const auto* selection = m_trackSelection->selectedSelection();
    if(!selection || selection->tracks.empty()) {
        return;
    }

    const auto services = runServices(*m_settings);
    const auto service
        = std::ranges::find_if(services, [&id](const RunService& candidate) { return actionId(candidate) == id; });
    if(service != services.cend()) {
        runService(*service, selection->tracks);
    }
}

void RunServicesPlugin::runService(const RunService& service, const TrackList& tracks)
{
    const auto commands = evaluateRunServiceCommands(service, tracks);
    if(commands.empty()) {
        qCWarning(RUN_SERVICES) << "Service produced no runnable command:" << service.name;
        return;
    }

    for(const QString& text : commands) {
        const auto command = resolveRunServiceCommand(text);
        if(!command) {
            qCWarning(RUN_SERVICES) << "Could not parse service command:" << text;
            continue;
        }
        if(!launchRunServiceCommand(*command)) {
            qCWarning(RUN_SERVICES) << "Could not launch service command:" << text;
        }
    }
}

QAction* RunServicesPlugin::registeredAction(const Id& id) const
{
    if(const auto it = m_runServiceActions.find(id); it != m_runServiceActions.cend()) {
        return it->second;
    }
    if(const auto* command = m_actionManager->command(id)) {
        return command->action();
    }
    return nullptr;
}

void RunServicesPlugin::syncRegisteredAction(const Id& id, const QString& text, bool active)
{
    QAction* action = registeredAction(id);
    if(!action) {
        return;
    }

    action->setText(text);
    action->setEnabled(active);
    action->setVisible(active);

    if(Command* command = m_actionManager->command(id)) {
        command->setDescription(text);
    }

    m_runServiceActions[id] = action;
}
} // namespace Fooyin::RunServices

#include "moc_runservicesplugin.cpp"
