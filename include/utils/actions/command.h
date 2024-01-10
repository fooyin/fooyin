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

#pragma once

#include "fyutils_export.h"

#include <utils/actions/proxyaction.h>
#include <utils/actions/widgetcontext.h>

#include <QObject>

class QAction;

namespace Fooyin {
class Id;

using ShortcutList = QList<QKeySequence>;

class FYUTILS_EXPORT Command : public QObject
{
    Q_OBJECT

public:
    [[nodiscard]] virtual Id id() const = 0;

    [[nodiscard]] virtual QAction* action() const                            = 0;
    [[nodiscard]] virtual QAction* actionForContext(const Id& context) const = 0;

    [[nodiscard]] virtual Context context() const = 0;

    virtual void setAttribute(ProxyAction::Attribute attribute)                     = 0;
    virtual void removeAttribute(ProxyAction::Attribute attribute)                  = 0;
    [[nodiscard]] virtual bool hasAttribute(ProxyAction::Attribute attribute) const = 0;

    [[nodiscard]] virtual bool isActive() const = 0;

    virtual void setShortcut(const ShortcutList& keys)                         = 0;
    [[nodiscard]] virtual QString stringWithShortcut(const QString& str) const = 0;
    virtual void actionWithShortcutToolTip(QAction* action) const              = 0;

    virtual void setDefaultShortcut(const QKeySequence& key)    = 0;
    virtual void setDefaultShortcut(const ShortcutList& keys)   = 0;
    [[nodiscard]] virtual ShortcutList defaultShortcuts() const = 0;
    [[nodiscard]] virtual ShortcutList shortcuts() const        = 0;
    [[nodiscard]] virtual QKeySequence shortcut() const         = 0;

    virtual void setDescription(const QString& text)  = 0;
    [[nodiscard]] virtual QString description() const = 0;

signals:
    void shortcutChanged();
    void activeStateChanged();
};
} // namespace Fooyin
