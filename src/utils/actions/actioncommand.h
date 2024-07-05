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

#pragma once

#include <utils/actions/command.h>

namespace Fooyin {
class ActionCommand : public Command
{
    Q_OBJECT

public:
    explicit ActionCommand(const Id& id);
    ~ActionCommand() override;

    [[nodiscard]] Id id() const override;

    [[nodiscard]] QAction* action() const override;
    [[nodiscard]] QAction* actionForContext(const Id& context) const override;

    [[nodiscard]] Context context() const override;

    void setAttribute(ProxyAction::Attribute attribute) override;
    void removeAttribute(ProxyAction::Attribute attribute) override;
    [[nodiscard]] bool hasAttribute(ProxyAction::Attribute attribute) const override;

    [[nodiscard]] bool isActive() const override;

    void setShortcut(const ShortcutList& shortcuts) override;
    [[nodiscard]] QString stringWithShortcut(const QString& str) const override;
    void actionWithShortcutToolTip(QAction* action) const override;

    void setDefaultShortcut(const QKeySequence& shortcut) override;
    void setDefaultShortcut(const ShortcutList& shortcuts) override;
    [[nodiscard]] ShortcutList defaultShortcuts() const override;
    [[nodiscard]] ShortcutList shortcuts() const override;
    [[nodiscard]] QKeySequence shortcut() const override;

    void setDescription(const QString& text) override;
    [[nodiscard]] QString description() const override;

    void setCurrentContext(const Context& newContext);
    void addOverrideAction(QAction* actionToAdd, const Context& context, bool changeContext = true);

private:
    void removeOverrideAction(QAction* actionToRemove);
    [[nodiscard]] bool isEmpty() const;
    void setActive(bool state);
    void updateActiveState();

    Id m_id;
    Context m_context;
    ShortcutList m_defaultKeys;
    QString m_defaultText;

    bool m_active{false};
    bool m_shortcutIsInitialised{false};

    std::map<Id, QPointer<QAction>> m_contextActionMap;
    ProxyAction* m_action{nullptr};
};
} // namespace Fooyin
