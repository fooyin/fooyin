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

#include <QAction>

namespace Fooyin {
class FYUTILS_EXPORT ProxyAction : public QAction
{
    Q_OBJECT

public:
    enum Attribute
    {
        Hide       = 1 << 0,
        UpdateText = 1 << 1,
        UpdateIcon = 1 << 2,
    };
    Q_DECLARE_FLAGS(Attributes, Attribute)

    explicit ProxyAction(QObject* parent = nullptr);
    ~ProxyAction() override;

    void initialise(QAction* action);

    [[nodiscard]] QAction* action() const;
    [[nodiscard]] bool shortcutVisibleInToolTip() const;

    void setAction(QAction* action);
    void setShortcutVisibleInToolTip(bool visible);

    void setAttribute(Attribute attribute);
    void removeAttribute(Attribute attribute);
    bool hasAttribute(Attribute attribute);

    static ProxyAction* actionWithIcon(QAction* original, const QIcon& icon);

signals:
    void currentActionChanged(QAction* action);

private:
    struct Private;
    std::unique_ptr<Private> p;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ProxyAction::Attributes)
} // namespace Fooyin
