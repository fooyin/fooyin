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

#include <utils/actions/command.h>

#include <utils/actions/proxyaction.h>
#include <utils/actions/widgetcontext.h>
#include <utils/id.h>
#include <utils/utils.h>

#include <QKeySequence>
#include <QLoggingCategory>
#include <QPointer>

namespace Fooyin {
class CommandPrivate : public QObject
{
    Q_OBJECT

public:
    CommandPrivate(Command* self, const Id& id)
        : m_self{self}
        , m_id{id}
        , m_action{new ProxyAction(m_self)}
    {
        m_action->setShortcutVisibleInToolTip(true);
        QObject::connect(m_action, &QAction::changed, this, &CommandPrivate::updateActiveState);
    }

    [[nodiscard]] bool isEmpty() const;

    void setActive(bool state);
    void updateActiveState();

    void removeOverrideAction(QAction* actionToRemove);

    Command* m_self;

    Id m_id;
    Context m_context;
    ShortcutList m_defaultKeys;
    QString m_defaultText;

    bool m_active{false};
    bool m_shortcutIsInitialised{false};

    std::map<Id, QPointer<QAction>> m_contextActionMap;
    ProxyAction* m_action{nullptr};
};

void CommandPrivate::removeOverrideAction(QAction* actionToRemove)
{
    std::erase_if(m_contextActionMap,
                  [actionToRemove](const auto& pair) { return !pair.second || pair.second == actionToRemove; });
    m_self->setCurrentContext(m_context);
}

[[nodiscard]] bool CommandPrivate::isEmpty() const
{
    return m_contextActionMap.empty();
}

void CommandPrivate::setActive(bool state)
{
    if(std::exchange(m_active, state) != state) {
        QMetaObject::invokeMethod(m_self, &Command::activeStateChanged);
    }
}

void CommandPrivate::updateActiveState()
{
    setActive(m_action->isEnabled() && m_action->isVisible() && !m_action->isSeparator());
}

Command::Command(const Id& id)
    : p{std::make_unique<CommandPrivate>(this, id)}
{ }

Command::~Command() = default;

Id Command::id() const
{
    return p->m_id;
}

QAction* Command::action() const
{
    return p->m_action;
}

QAction* Command::actionForContext(const Id& context) const
{
    if(p->m_contextActionMap.contains(context)) {
        return p->m_contextActionMap.at(context);
    }
    return nullptr;
}

Context Command::context() const
{
    return p->m_context;
}

void Command::setAttribute(ProxyAction::Attribute attribute)
{
    p->m_action->setAttribute(attribute);
}

void Command::removeAttribute(ProxyAction::Attribute attribute)
{
    p->m_action->removeAttribute(attribute);
}

bool Command::hasAttribute(ProxyAction::Attribute attribute) const
{
    return p->m_action->hasAttribute(attribute);
}

bool Command::isActive() const
{
    return p->m_active;
}

void Command::setShortcut(const ShortcutList& keys)
{
    p->m_shortcutIsInitialised = true;
    p->m_action->setShortcuts(keys);
    emit shortcutChanged();
}

QString Command::stringWithShortcut(const QString& str) const
{
    return Utils::appendShortcut(str, shortcut());
}

void Command::actionWithShortcutToolTip(QAction* action) const
{
    action->setToolTip(stringWithShortcut(action->text()));
    QObject::connect(this, &Command::shortcutChanged, action,
                     [this, action] { action->setToolTip(stringWithShortcut(action->text())); });
    QObject::connect(action, &QAction::changed, this,
                     [this, action] { action->setToolTip(stringWithShortcut(action->text())); });
}

void Command::setDefaultShortcut(const QKeySequence& shortcut)
{
    if(!p->m_shortcutIsInitialised) {
        setShortcut({shortcut});
    }
    p->m_defaultKeys = {shortcut};
}

void Command::setDefaultShortcut(const ShortcutList& shortcuts)
{
    if(!p->m_shortcutIsInitialised) {
        setShortcut(shortcuts);
    }
    p->m_defaultKeys = shortcuts;
}

ShortcutList Command::defaultShortcuts() const
{
    return p->m_defaultKeys;
}

ShortcutList Command::shortcuts() const
{
    return p->m_action->shortcuts();
}

QKeySequence Command::shortcut() const
{
    return p->m_action->shortcut();
}

void Command::setDescription(const QString& text)
{
    p->m_defaultText = text;
}

QString Command::description() const
{
    if(!p->m_defaultText.isEmpty()) {
        return p->m_defaultText;
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

void Command::setCurrentContext(const Context& context)
{
    p->m_context = context;

    QAction* currentAction = nullptr;
    for(const Id& contextId : std::as_const(p->m_context)) {
        if(p->m_contextActionMap.contains(contextId)) {
            if(QAction* contextAction = p->m_contextActionMap.at(contextId)) {
                currentAction = contextAction;
                break;
            }
        }
    }

    p->m_action->setAction(currentAction);
    p->updateActiveState();
}

void Command::addOverrideAction(QAction* action, const Context& context, bool changeContext)
{
    if(action->menuRole() == QAction::TextHeuristicRole) {
        action->setMenuRole(QAction::NoRole);
    }

    if(p->isEmpty()) {
        p->m_action->initialise(action);
    }

    QObject::connect(action, &QObject::destroyed, this, [this, action]() { p->removeOverrideAction(action); });

    if(context.empty()) {
        p->m_contextActionMap.emplace(Constants::Context::Global, action);
    }
    else {
        for(const Id& contextId : context) {
            if(p->m_contextActionMap.contains(contextId)) {
                if(auto contextAction = p->m_contextActionMap.at(contextId)) {
                    QLoggingCategory log{"Actions"};
                    qCWarning(log) << "Context" << contextId.name() << "already added for" << contextAction;
                }
            }
            p->m_contextActionMap.emplace(contextId, action);
        }
    }

    if(changeContext) {
        setCurrentContext(context);
    }
}
} // namespace Fooyin

#include "utils/actions/command.moc"
#include "utils/actions/moc_command.cpp"
