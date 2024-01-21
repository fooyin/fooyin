/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

#include <utils/actions/actionmanager.h>

#include "actions/actioncommand.h"
#include "menucontainer.h"

#include <utils/actions/command.h>
#include <utils/settings/settingsmanager.h>

#include <QAction>
#include <QApplication>
#include <QIODevice>
#include <QMainWindow>
#include <QMenuBar>
#include <QSettings>

#include <set>

constexpr auto ShortcutCategory = "KeyboardShortcuts/";

namespace Fooyin {
struct ActionManager::Private
{
    ActionManager* self;

    SettingsManager* settingsManager;
    QMainWindow* mainWindow{nullptr};

    Context currentContext;

    std::unordered_map<Id, std::unique_ptr<ActionCommand>, Id::IdHash> idCmdMap;
    std::unordered_map<Id, std::unique_ptr<MenuContainer>, Id::IdHash> idContainerMap;
    std::set<MenuContainer*> scheduledContainerUpdates;

    WidgetContextList activeContext;
    std::unordered_map<QWidget*, WidgetContext*> contextWidgets;

    explicit Private(ActionManager* self, SettingsManager* settingsManager)
        : self{self}
        , settingsManager{settingsManager}
    { }

    ActionCommand* overridableAction(const Id& id)
    {
        if(idCmdMap.contains(id)) {
            return idCmdMap.at(id).get();
        }

        auto* command = idCmdMap.emplace(id, std::make_unique<ActionCommand>(id)).first->second.get();
        loadSetting(id, command);
        QAction* action = command->action();
        mainWindow->addAction(action);
        action->setObjectName(id.name());
        action->setShortcutContext(Qt::ApplicationShortcut);
        command->setCurrentContext(currentContext);

        return command;
    }

    void loadSetting(const Id& id, Command* command) const
    {
        const QString key = ShortcutCategory + id.name();

        if(settingsManager->fileContains(key)) {
            const QVariant var = settingsManager->fileValue(key);
            if(QMetaType::Type(var.typeId()) == QMetaType::QStringList) {
                ShortcutList shortcuts;
                std::ranges::transform(var.toStringList(), std::back_inserter(shortcuts),
                                       [](const QKeySequence& k) { return k.toString(); });
                command->setShortcut(shortcuts);
            }
            else {
                command->setShortcut({QKeySequence::fromString(var.toString())});
            }
        }
    }

    void containerDestroyed(QObject* sender)
    {
        if(auto* container = qobject_cast<MenuContainer*>(sender)) {
            idContainerMap.erase(container->id());
            scheduledContainerUpdates.erase(container);
        }
    }

    void updateContainer()
    {
        for(MenuContainer* container : scheduledContainerUpdates) {
            container->update();
        }
        scheduledContainerUpdates.clear();
    }

    void scheduleContainerUpdate(MenuContainer* actionContainer)
    {
        const bool needsSchedule = scheduledContainerUpdates.empty();
        scheduledContainerUpdates.emplace(actionContainer);
        if(needsSchedule) {
            QMetaObject::invokeMethod(
                self, [this]() { updateContainer(); }, Qt::QueuedConnection);
        }
    }

    void updateContextObject(const WidgetContextList& context)
    {
        activeContext = context;

        Context uniqueContexts;
        for(WidgetContext* ctx : activeContext) {
            for(const Id& id : ctx->context()) {
                uniqueContexts.append(id);
            }
        }

        uniqueContexts.append(Constants::Context::Global);

        setContext(uniqueContexts);
        QMetaObject::invokeMethod(self, "contextChanged", Q_ARG(const Context&, uniqueContexts));
    }

    void updateFocusWidget(QWidget* widget)
    {
        if(qobject_cast<QMenuBar*>(widget) || qobject_cast<QMenu*>(widget)) {
            return;
        }

        WidgetContextList newContext;

        if(QWidget* focusedWidget = QApplication::focusWidget()) {
            WidgetContext* widgetContext{nullptr};
            while(focusedWidget) {
                widgetContext = self->contextObject(focusedWidget);
                if(widgetContext) {
                    newContext.push_back(widgetContext);
                }
                focusedWidget = focusedWidget->parentWidget();
            }
        }

        if(!newContext.empty() || QApplication::focusWidget() == mainWindow->focusWidget()) {
            updateContextObject(newContext);
        }
    }

    void setContext(const Context& updatedContext)
    {
        currentContext = updatedContext;
        for(const auto& [id, command] : idCmdMap) {
            command->setCurrentContext(currentContext);
        }
    }
};

ActionManager::ActionManager(SettingsManager* settingsManager, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(this, settingsManager)}
{
    QObject::connect(qApp, &QApplication::focusChanged, this,
                     [this](QWidget* /*old*/, QWidget* now) { p->updateFocusWidget(now); });
}

ActionManager::~ActionManager()
{
    QObject::disconnect(qApp, &QApplication::focusChanged, this, nullptr);

    for(auto [_, context] : p->contextWidgets) {
        context->disconnect();
    }

    p->contextWidgets.clear();
    p->activeContext.clear();

    for(const auto& [_, container] : p->idContainerMap) {
        container->disconnect();
    }

    p->idContainerMap.clear();
    p->idCmdMap.clear();
}

void ActionManager::setMainWindow(QMainWindow* mainWindow)
{
    p->mainWindow = mainWindow;
}

void ActionManager::saveSettings()
{
    for(const auto& [_, command] : p->idCmdMap) {
        const QString key = ShortcutCategory + command->id().name();

        const ShortcutList commandShortcuts = command->shortcuts();
        const ShortcutList defaultShortcuts = command->defaultShortcuts();
        if(commandShortcuts != defaultShortcuts) {
            // Only save user changes
            QStringList keys;
            std::ranges::transform(commandShortcuts, std::back_inserter(keys),
                                   [](const QKeySequence& k) { return k.toString(); });
            p->settingsManager->fileSet(key, keys);
        }
        else {
            p->settingsManager->fileRemove(key);
        }
    }
}

WidgetContext* ActionManager::currentContextObject() const
{
    return p->activeContext.empty() ? nullptr : p->activeContext.front();
}

QWidget* ActionManager::currentContextWidget() const
{
    WidgetContext* context = currentContextObject();
    return context ? context->widget() : nullptr;
}

WidgetContext* ActionManager::contextObject(QWidget* widget) const
{
    if(p->contextWidgets.contains(widget)) {
        return p->contextWidgets.at(widget);
    }
    return nullptr;
}

void ActionManager::addContextObject(WidgetContext* context)
{
    if(!context) {
        return;
    }

    QWidget* widget = context->widget();
    if(p->contextWidgets.contains(widget)) {
        return;
    }

    p->contextWidgets.emplace(widget, context);
    QObject::connect(context, &QObject::destroyed, this, [this, context] { removeContextObject(context); });
}

void ActionManager::removeContextObject(WidgetContext* context)
{
    if(!context) {
        return;
    }

    QObject::disconnect(context, &QObject::destroyed, this, nullptr);

    if(!std::erase_if(p->contextWidgets, [context](const auto& v) { return v.second == context; })) {
        return;
    }

    if(std::erase_if(p->activeContext, [context](WidgetContext* wc) { return wc == context; }) > 0) {
        p->updateContextObject(p->activeContext);
    }
}

ActionContainer* ActionManager::createMenu(const Id& id)
{
    if(p->idContainerMap.contains(id)) {
        return p->idContainerMap.at(id).get();
    }

    auto* container
        = p->idContainerMap.emplace(id, std::make_unique<MenuActionContainer>(id, this)).first->second.get();

    QObject::connect(container, &QObject::destroyed, this, [this](QObject* sender) { p->containerDestroyed(sender); });
    QObject::connect(container, &MenuContainer::requestUpdate, this,
                     [this](MenuContainer* container) { p->scheduleContainerUpdate(container); });

    container->appendGroup(Actions::Groups::One);
    container->appendGroup(Actions::Groups::Two);
    container->appendGroup(Actions::Groups::Three);

    return container;
}

ActionContainer* ActionManager::createMenuBar(const Id& id)
{
    if(p->idContainerMap.contains(id)) {
        return p->idContainerMap.at(id).get();
    }

    {
        auto* menuBar = new QMenuBar(p->mainWindow);
        menuBar->setObjectName(id.name());

        auto container = std::make_unique<MenuBarActionContainer>(id, this);
        container->setMenuBar(menuBar);
        p->idContainerMap.emplace(id, std::move(container));
    }

    auto* container = p->idContainerMap.at(id).get();

    QObject::connect(container, &QObject::destroyed, this, [this](QObject* sender) { p->containerDestroyed(sender); });
    QObject::connect(container, &MenuContainer::requestUpdate, this,
                     [this](MenuContainer* container) { p->scheduleContainerUpdate(container); });

    return container;
}

Command* ActionManager::registerAction(QAction* action, const Id& id, const Context& context)
{
    ActionCommand* command = p->overridableAction(id);
    if(command) {
        command->addOverrideAction(action, context);
        emit commandsChanged();
    }
    return command;
}

Command* ActionManager::command(const Id& id) const
{
    if(p->idCmdMap.contains(id)) {
        return p->idCmdMap.at(id).get();
    }
    return nullptr;
}

CommandList ActionManager::commands() const
{
    CommandList commands;
    std::ranges::transform(std::as_const(p->idCmdMap), std::back_inserter(commands),
                           [&](const auto& idCommand) { return idCommand.second.get(); });
    return commands;
}

ActionContainer* ActionManager::actionContainer(const Id& id) const
{
    if(p->idContainerMap.contains(id)) {
        return p->idContainerMap.at(id).get();
    }
    return nullptr;
}
} // namespace Fooyin

#include "utils/actions/moc_actionmanager.cpp"
