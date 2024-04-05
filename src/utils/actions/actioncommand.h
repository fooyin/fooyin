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
    ActionCommand(const Id& id);
    ~ActionCommand() override;

    Id id() const override;

    QAction* action() const override;
    QAction* actionForContext(const Id& context) const override;

    Context context() const override;

    void setAttribute(ProxyAction::Attribute attribute) override;
    void removeAttribute(ProxyAction::Attribute attribute) override;
    bool hasAttribute(ProxyAction::Attribute attribute) const override;

    bool isActive() const override;

    void setShortcut(const ShortcutList& shortcuts) override;
    QString stringWithShortcut(const QString& str) const override;
    void actionWithShortcutToolTip(QAction* action) const override;

    void setDefaultShortcut(const QKeySequence& shortcut) override;
    void setDefaultShortcut(const ShortcutList& shortcuts) override;
    ShortcutList defaultShortcuts() const override;
    ShortcutList shortcuts() const override;
    QKeySequence shortcut() const override;

    void setDescription(const QString& text) override;
    QString description() const override;

    void setCurrentContext(const Context& newContext);
    void addOverrideAction(QAction* actionToAdd, const Context& context, bool changeContext = true);

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin
