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

#include "fyutils_export.h"

#include <utils/actions/proxyaction.h>
#include <utils/actions/widgetcontext.h>

#include <QObject>

class QAction;

namespace Fooyin {
class CommandPrivate;
class Id;

using ShortcutList = QList<QKeySequence>;

class FYUTILS_EXPORT Command : public QObject
{
    Q_OBJECT

public:
    explicit Command(const Id& id);
    ~Command() override;

    [[nodiscard]] Id id() const;

    [[nodiscard]] QAction* action() const;
    [[nodiscard]] QAction* actionForContext(const Id& context) const;

    [[nodiscard]] Context context() const;

    void setAttribute(ProxyAction::Attribute attribute);
    void removeAttribute(ProxyAction::Attribute attribute);
    [[nodiscard]] bool hasAttribute(ProxyAction::Attribute attribute) const;

    [[nodiscard]] bool isActive() const;

    void setShortcut(const ShortcutList& keys);
    [[nodiscard]] QString stringWithShortcut(const QString& str) const;
    void actionWithShortcutToolTip(QAction* action) const;

    void setDefaultShortcut(const QKeySequence& key);
    void setDefaultShortcut(const ShortcutList& keys);
    [[nodiscard]] ShortcutList defaultShortcuts() const;
    [[nodiscard]] ShortcutList shortcuts() const;
    [[nodiscard]] QKeySequence shortcut() const;

    void setDescription(const QString& text);
    [[nodiscard]] QString description() const;

    void setCurrentContext(const Context& newContext);
    void addOverrideAction(QAction* actionToAdd, const Context& context, bool changeContext = true);

signals:
    void shortcutChanged();
    void activeStateChanged();

private:
    std::unique_ptr<CommandPrivate> p;
};
} // namespace Fooyin
