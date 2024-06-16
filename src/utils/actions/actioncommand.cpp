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

#include "actioncommand.h"

#include <utils/actions/proxyaction.h>
#include <utils/actions/widgetcontext.h>
#include <utils/id.h>
#include <utils/utils.h>

#include <QKeySequence>
#include <QPointer>

namespace Fooyin {
ActionCommand::ActionCommand(const Id& id)
    : m_id{id}
    , m_action{new ProxyAction(this)}
{
    m_action->setShortcutVisibleInToolTip(true);
    connect(m_action, &QAction::changed, this, &ActionCommand::updateActiveState);
}

ActionCommand::~ActionCommand() = default;

Id ActionCommand::id() const
{
    return m_id;
}

QAction* ActionCommand::action() const
{
    return m_action;
}

QAction* ActionCommand::actionForContext(const Id& context) const
{
    if(m_contextActionMap.contains(context)) {
        return m_contextActionMap.at(context);
    }
    return nullptr;
}

Context ActionCommand::context() const
{
    return m_context;
}

void ActionCommand::setAttribute(ProxyAction::Attribute attribute)
{
    m_action->setAttribute(attribute);
}

void ActionCommand::removeAttribute(ProxyAction::Attribute attribute)
{
    m_action->removeAttribute(attribute);
}

bool ActionCommand::hasAttribute(ProxyAction::Attribute attribute) const
{
    return m_action->hasAttribute(attribute);
}

bool ActionCommand::isActive() const
{
    return m_active;
}

void ActionCommand::setShortcut(const ShortcutList& keys)
{
    m_shortcutIsInitialised = true;
    m_action->setShortcuts(keys);
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
    if(!m_shortcutIsInitialised) {
        setShortcut({shortcut});
    }
    m_defaultKeys = {shortcut};
}

void ActionCommand::setDefaultShortcut(const ShortcutList& shortcuts)
{
    if(!m_shortcutIsInitialised) {
        setShortcut(shortcuts);
    }
    m_defaultKeys = shortcuts;
}

ShortcutList ActionCommand::defaultShortcuts() const
{
    return m_defaultKeys;
}

ShortcutList ActionCommand::shortcuts() const
{
    return m_action->shortcuts();
}

QKeySequence ActionCommand::shortcut() const
{
    return m_action->shortcut();
}

void ActionCommand::setDescription(const QString& text)
{
    m_defaultText = text;
}

QString ActionCommand::description() const
{
    if(!m_defaultText.isEmpty()) {
        return m_defaultText;
    }
    if(const QAction* act = action()) {
        QString text = act->text();
        text.remove(QStringLiteral("&"));

        if(!text.isEmpty()) {
            return text;
        }
    }
    return id().name();
}

void ActionCommand::setCurrentContext(const Context& context)
{
    m_context = context;

    QAction* currentAction = nullptr;
    for(const Id& contextId : std::as_const(m_context)) {
        if(m_contextActionMap.contains(contextId)) {
            if(QAction* contextAction = m_contextActionMap.at(contextId)) {
                currentAction = contextAction;
                break;
            }
        }
    }

    m_action->setAction(currentAction);
    updateActiveState();
}

void ActionCommand::addOverrideAction(QAction* action, const Context& context, bool changeContext)
{
    if(action->menuRole() == QAction::TextHeuristicRole) {
        action->setMenuRole(QAction::NoRole);
    }

    if(isEmpty()) {
        m_action->initialise(action);
    }

    QObject::connect(action, &QObject::destroyed, this, [this, action]() { removeOverrideAction(action); });

    if(context.empty()) {
        m_contextActionMap.emplace(Constants::Context::Global, action);
    }
    else {
        for(const Id& contextId : context) {
            if(m_contextActionMap.contains(contextId)) {
                if(auto contextAction = m_contextActionMap.at(contextId)) {
                    qWarning() << "Context " << contextId.name() << " already added for " << contextAction;
                }
            }
            m_contextActionMap.emplace(contextId, action);
        }
    }

    if(changeContext) {
        setCurrentContext(context);
    }
}

void ActionCommand::removeOverrideAction(QAction* actionToRemove)
{
    std::erase_if(m_contextActionMap,
                  [actionToRemove](const auto& pair) { return !pair.second || pair.second == actionToRemove; });
    setCurrentContext(m_context);
}

[[nodiscard]] bool ActionCommand::isEmpty() const
{
    return m_contextActionMap.empty();
}

void ActionCommand::setActive(bool state)
{
    if(std::exchange(m_active, state) != state) {
        QMetaObject::invokeMethod(this, &Command::activeStateChanged);
    }
}

void ActionCommand::updateActiveState()
{
    setActive(m_action->isEnabled() && m_action->isVisible() && !m_action->isSeparator());
}
} // namespace Fooyin

#include "moc_actioncommand.cpp"
