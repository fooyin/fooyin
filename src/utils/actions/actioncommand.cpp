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

#include "actioncommand.h"

#include <utils/actions/proxyaction.h>
#include <utils/actions/widgetcontext.h>
#include <utils/id.h>
#include <utils/utils.h>

#include <QKeySequence>
#include <QPointer>

namespace Fooyin {
struct ActionCommand::Private
{
    ActionCommand* self;

    Id id;
    Context context;
    ShortcutList defaultKeys;
    QString defaultText;

    bool shortcutIsInitialised{false};

    ProxyAction* action{nullptr};

    std::map<Id, QPointer<QAction>> contextActionMap;

    bool active{false};

    Private(ActionCommand* self, const Id& id)
        : self{self}
        , id{id}
        , action{new ProxyAction(self)}
    {
        action->setShortcutVisibleInToolTip(true);
        connect(action, &QAction::changed, self, [this]() { updateActiveState(); });
    }

    void removeOverrideAction(QAction* actionToRemove)
    {
        std::erase_if(contextActionMap,
                      [actionToRemove](const auto& pair) { return !pair.second || pair.second == actionToRemove; });
        self->setCurrentContext(context);
    }

    [[nodiscard]] bool isEmpty() const
    {
        return contextActionMap.empty();
    }

    void setActive(bool state)
    {
        if(std::exchange(active, state) != state) {
            QMetaObject::invokeMethod(self, &Command::activeStateChanged);
        }
    }

    void updateActiveState()
    {
        setActive(action->isEnabled() && action->isVisible() && !action->isSeparator());
    }
};

ActionCommand::ActionCommand(const Id& id)
    : p{std::make_unique<Private>(this, id)}
{ }

ActionCommand::~ActionCommand() = default;

Id ActionCommand::id() const
{
    return p->id;
}

QAction* ActionCommand::action() const
{
    return p->action;
}

QAction* ActionCommand::actionForContext(const Id& context) const
{
    if(p->contextActionMap.contains(context)) {
        return p->contextActionMap.at(context);
    }
    return nullptr;
}

Context ActionCommand::context() const
{
    return p->context;
}

void ActionCommand::setAttribute(ProxyAction::Attribute attribute)
{
    p->action->setAttribute(attribute);
}

void ActionCommand::removeAttribute(ProxyAction::Attribute attribute)
{
    p->action->removeAttribute(attribute);
}

bool ActionCommand::hasAttribute(ProxyAction::Attribute attribute) const
{
    return p->action->hasAttribute(attribute);
}

bool ActionCommand::isActive() const
{
    return p->active;
}

void ActionCommand::setShortcut(const ShortcutList& keys)
{
    p->shortcutIsInitialised = true;
    p->action->setShortcuts(keys);
    emit shortcutChanged();
}

QString ActionCommand::stringWithShortcut(const QString& str) const
{
    return Utils::appendShortcut(str, shortcut());
}

void ActionCommand::actionWithShortcutToolTip(QAction* action) const
{
    action->setToolTip(stringWithShortcut(action->text()));
    QObject::connect(this, &Command::shortcutChanged, action,
                     [this, action] { action->setToolTip(stringWithShortcut(action->text())); });
    QObject::connect(action, &QAction::changed, this,
                     [this, action] { action->setToolTip(stringWithShortcut(action->text())); });
}

void ActionCommand::setDefaultShortcut(const QKeySequence& shortcut)
{
    if(!p->shortcutIsInitialised) {
        setShortcut({shortcut});
    }
    p->defaultKeys = {shortcut};
}

void ActionCommand::setDefaultShortcut(const ShortcutList& shortcuts)
{
    if(!p->shortcutIsInitialised) {
        setShortcut(shortcuts);
    }
    p->defaultKeys = shortcuts;
}

ShortcutList ActionCommand::defaultShortcuts() const
{
    return p->defaultKeys;
}

ShortcutList ActionCommand::shortcuts() const
{
    return p->action->shortcuts();
}

QKeySequence ActionCommand::shortcut() const
{
    return p->action->shortcut();
}

void ActionCommand::setDescription(const QString& text)
{
    p->defaultText = text;
}

QString ActionCommand::description() const
{
    if(!p->defaultText.isEmpty()) {
        return p->defaultText;
    }
    if(const QAction* act = action()) {
        QString text = act->text();
        text.remove('&');

        if(!text.isEmpty()) {
            return text;
        }
    }
    return id().name();
}

void ActionCommand::setCurrentContext(const Context& context)
{
    p->context = context;

    QAction* currentAction = nullptr;
    for(const Id& contextId : std::as_const(p->context)) {
        if(p->contextActionMap.contains(contextId)) {
            if(QAction* contextAction = p->contextActionMap.at(contextId)) {
                currentAction = contextAction;
                break;
            }
        }
    }

    p->action->setAction(currentAction);
    p->updateActiveState();
}

void ActionCommand::addOverrideAction(QAction* action, const Context& context)
{
    if(action->menuRole() == QAction::TextHeuristicRole) {
        action->setMenuRole(QAction::NoRole);
    }

    if(p->isEmpty()) {
        p->action->initialise(action);
    }

    QObject::connect(action, &QObject::destroyed, this, [this, action]() { p->removeOverrideAction(action); });

    if(context.empty()) {
        p->contextActionMap.emplace(Constants::Context::Global, action);
    }
    else {
        for(const Id& contextId : context) {
            if(p->contextActionMap.contains(contextId)) {
                if(auto contextAction = p->contextActionMap.at(contextId)) {
                    qWarning() << "Context " << contextId.name() << " already added for " << contextAction;
                }
            }
            p->contextActionMap.emplace(contextId, action);
        }
    }
    setCurrentContext(context);
}
} // namespace Fooyin

#include "moc_actioncommand.cpp"
