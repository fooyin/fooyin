/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "menucontainer.h"

#include <utils/actions/command.h>
#include <utils/settings/settingsmanager.h>

#include <QAction>
#include <QApplication>
#include <QIODevice>
#include <QMainWindow>
#include <QMenuBar>

#include <set>

namespace Fooyin {
class ActionManagerPrivate
{
public:
    explicit ActionManagerPrivate(ActionManager* self, SettingsManager* settingsManager)
        : m_self{self}
        , m_settingsManager{settingsManager}
    { }

    Command* overridableAction(const Id& id);
    void loadSetting(const Id& id, Command* command) const;

    void updateContainer();
    void scheduleContainerUpdate(ActionContainer* actionContainer);

    void updateContextObject(const WidgetContextList& context);
    void updateFocusWidget(QWidget* widget);
    void setContext(const Context& updatedContext);

    ActionManager* m_self;

    SettingsManager* m_settingsManager;
    QMainWindow* m_mainWindow{nullptr};

    std::unordered_map<Id, std::unique_ptr<Command>, Id::IdHash> m_idCmdMap;
    std::unordered_map<Id, std::unique_ptr<ActionContainer>, Id::IdHash> m_idContainerMap;
    std::unordered_map<QWidget*, WidgetContext*> m_contextWidgets;
    std::set<ActionContainer*> m_scheduledContainerUpdates;

    Context m_currentContext;
    bool m_contextOverride{false};
    WidgetContext* m_widgetOverride{nullptr};
    WidgetContextList m_activeContext;
};

Command* ActionManagerPrivate::overridableAction(const Id& id)
{
    if(m_idCmdMap.contains(id)) {
        return m_idCmdMap.at(id).get();
    }

    auto* command = m_idCmdMap.emplace(id, std::make_unique<Command>(id)).first->second.get();
    loadSetting(id, command);
    QAction* action = command->action();
    m_mainWindow->addAction(action);
    action->setObjectName(id.name());
    action->setShortcutContext(Qt::ApplicationShortcut);
    command->setCurrentContext(m_currentContext);

    return command;
}

void ActionManagerPrivate::loadSetting(const Id& id, Command* command) const
{
    const QString key = QStringLiteral("KeyboardShortcuts/") + id.name();

    if(m_settingsManager->fileContains(key)) {
        const QVariant var = m_settingsManager->fileValue(key);
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

void ActionManagerPrivate::updateContainer()
{
    for(auto* container : m_scheduledContainerUpdates) {
        container->update();
    }
    m_scheduledContainerUpdates.clear();
}

void ActionManagerPrivate::scheduleContainerUpdate(ActionContainer* actionContainer)
{
    const bool needsSchedule = m_scheduledContainerUpdates.empty();
    m_scheduledContainerUpdates.emplace(actionContainer);
    if(needsSchedule) {
        QMetaObject::invokeMethod(m_self, [this]() { updateContainer(); }, Qt::QueuedConnection);
    }
}

void ActionManagerPrivate::updateContextObject(const WidgetContextList& context)
{
    m_activeContext = context;

    Context uniqueContexts;
    for(WidgetContext* ctx : m_activeContext) {
        for(const Id& id : ctx->context()) {
            uniqueContexts.append(id);
        }
    }

    uniqueContexts.append(Constants::Context::Global);

    setContext(uniqueContexts);
    emit m_self->contextChanged(uniqueContexts);
}

void ActionManagerPrivate::updateFocusWidget(QWidget* widget)
{
    if(qobject_cast<QMenuBar*>(widget) || qobject_cast<QMenu*>(widget)) {
        return;
    }

    if(m_contextOverride) {
        return;
    }

    WidgetContextList newContext;

    if(QWidget* focusedWidget = QApplication::focusWidget()) {
        while(focusedWidget) {
            if(auto* widgetContext = m_self->contextObject(focusedWidget)) {
                if(widgetContext->isEnabled()) {
                    newContext.push_back(widgetContext);
                }
            }
            focusedWidget = focusedWidget->parentWidget();
        }
    }

    if(!newContext.empty() || QApplication::focusWidget() == m_mainWindow->focusWidget()) {
        updateContextObject(newContext);
    }
}

void ActionManagerPrivate::setContext(const Context& updatedContext)
{
    m_currentContext = updatedContext;
    for(const auto& [id, command] : m_idCmdMap) {
        command->setCurrentContext(m_currentContext);
    }
}

ActionManager::ActionManager(SettingsManager* settingsManager, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<ActionManagerPrivate>(this, settingsManager)}
{
    QObject::connect(qApp, &QApplication::focusChanged, this,
                     [this](QWidget* /*old*/, QWidget* now) { p->updateFocusWidget(now); });
}

ActionManager::~ActionManager()
{
    QObject::disconnect(qApp, &QApplication::focusChanged, this, nullptr);

    for(auto [_, context] : p->m_contextWidgets) {
        context->disconnect();
    }

    p->m_contextWidgets.clear();
    p->m_activeContext.clear();

    for(const auto& [_, container] : p->m_idContainerMap) {
        container->disconnect();
    }

    p->m_idContainerMap.clear();
    p->m_idCmdMap.clear();
}

void ActionManager::setMainWindow(QMainWindow* mainWindow)
{
    p->m_mainWindow = mainWindow;
}

void ActionManager::saveSettings()
{
    for(const auto& [_, command] : p->m_idCmdMap) {
        const QString key = QStringLiteral("KeyboardShortcuts/") + command->id().name();

        const ShortcutList commandShortcuts = command->shortcuts();
        const ShortcutList defaultShortcuts = command->defaultShortcuts();
        if(commandShortcuts != defaultShortcuts) {
            // Only save user changes
            QStringList keys;
            std::ranges::transform(commandShortcuts, std::back_inserter(keys),
                                   [](const QKeySequence& k) { return k.toString(); });
            p->m_settingsManager->fileSet(key, keys);
        }
        else {
            p->m_settingsManager->fileRemove(key);
        }
    }
}

WidgetContext* ActionManager::currentContextObject() const
{
    return p->m_activeContext.empty() ? nullptr : p->m_activeContext.front();
}

QWidget* ActionManager::currentContextWidget() const
{
    WidgetContext* context = currentContextObject();
    return context ? context->widget() : nullptr;
}

WidgetContext* ActionManager::contextObject(QWidget* widget) const
{
    if(p->m_contextWidgets.contains(widget)) {
        return p->m_contextWidgets.at(widget);
    }
    return nullptr;
}

void ActionManager::addContextObject(WidgetContext* context)
{
    if(!context) {
        return;
    }

    QWidget* widget = context->widget();
    if(p->m_contextWidgets.contains(widget)) {
        return;
    }

    p->m_contextWidgets.emplace(widget, context);
    QObject::connect(context, &WidgetContext::isEnabledChanged, this,
                     [this] { p->updateFocusWidget(QApplication::focusWidget()); });
    QObject::connect(context, &QObject::destroyed, this, [this, context] { removeContextObject(context); });
}

void ActionManager::overrideContext(WidgetContext* context, bool override)
{
    if(!context) {
        return;
    }

    if(override && p->m_contextOverride) {
        // Only one override allowed
        return;
    }

    if(override) {
        p->m_contextOverride = true;
        p->m_widgetOverride  = context;
        p->updateContextObject({context});
    }
    else if(context == p->m_widgetOverride) {
        p->m_contextOverride = false;
        p->m_widgetOverride  = nullptr;
        p->m_activeContext.clear();
        p->m_currentContext = {};
        p->updateFocusWidget(QApplication::focusWidget());
    }
}

void ActionManager::removeContextObject(WidgetContext* context)
{
    if(!context) {
        return;
    }

    QObject::disconnect(context, &QObject::destroyed, this, nullptr);

    if(!std::erase_if(p->m_contextWidgets, [context](const auto& v) { return v.second == context; })) {
        return;
    }

    if(std::erase_if(p->m_activeContext, [context](WidgetContext* wc) { return wc == context; }) > 0) {
        p->updateContextObject(p->m_activeContext);
    }
}

ActionContainer* ActionManager::createMenu(const Id& id)
{
    if(p->m_idContainerMap.contains(id)) {
        return p->m_idContainerMap.at(id).get();
    }

    auto* menu = p->m_idContainerMap.emplace(id, std::make_unique<MenuContainer>(id, this)).first->second.get();

    QObject::connect(menu, &ActionContainer::requestUpdate, this,
                     [this](auto* container) { p->scheduleContainerUpdate(container); });

    menu->appendGroup(Actions::Groups::One);
    menu->appendGroup(Actions::Groups::Two);
    menu->appendGroup(Actions::Groups::Three);

    return menu;
}

ActionContainer* ActionManager::createMenuBar(const Id& id)
{
    if(p->m_idContainerMap.contains(id)) {
        return p->m_idContainerMap.at(id).get();
    }

    {
        auto* menuBar = new QMenuBar(p->m_mainWindow);
        menuBar->setObjectName(id.name());

        auto container = std::make_unique<MenuBarContainer>(id, this);
        container->setMenuBar(menuBar);
        p->m_idContainerMap.emplace(id, std::move(container));
    }

    auto* menuBar = p->m_idContainerMap.at(id).get();

    QObject::connect(menuBar, &ActionContainer::requestUpdate, this,
                     [this](auto* container) { p->scheduleContainerUpdate(container); });

    return menuBar;
}

Command* ActionManager::registerAction(QAction* action, const Id& id, const Context& context)
{
    Command* command = p->overridableAction(id);
    if(command) {
        command->addOverrideAction(action, context, !p->m_contextOverride);
        emit commandsChanged();
    }
    return command;
}

Command* ActionManager::command(const Id& id) const
{
    if(p->m_idCmdMap.contains(id)) {
        return p->m_idCmdMap.at(id).get();
    }
    return nullptr;
}

CommandList ActionManager::commands() const
{
    CommandList commands;
    std::ranges::transform(std::as_const(p->m_idCmdMap), std::back_inserter(commands),
                           [&](const auto& idCommand) { return idCommand.second.get(); });
    return commands;
}

ActionContainer* ActionManager::actionContainer(const Id& id) const
{
    if(p->m_idContainerMap.contains(id)) {
        return p->m_idContainerMap.at(id).get();
    }
    return nullptr;
}
} // namespace Fooyin

#include "utils/actions/moc_actionmanager.cpp"
