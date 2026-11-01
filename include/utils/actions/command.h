/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

enum class ShortcutTarget : uint8_t
{
    FocusedWidget = 0,
    NowPlaying,
};

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
    [[nodiscard]] bool hasOverrideActions() const;

    void setShortcut(const ShortcutList& keys);
    void setGlobalShortcuts(const ShortcutList& keys);

    //! Targets affect shortcut invocation only; menu actions retain their normal selection.
    [[nodiscard]] ShortcutTarget shortcutTarget(const QKeySequence& key) const;
    void setShortcutTarget(const QKeySequence& key, ShortcutTarget target);
    [[nodiscard]] ShortcutTarget globalShortcutTarget() const;
    void setGlobalShortcutTarget(ShortcutTarget target);

    //! Explicit targets require an action which honours the supplied command selection.
    [[nodiscard]] bool supportsShortcutTarget(ShortcutTarget target) const;
    void setSupportsShortcutTarget(ShortcutTarget target, bool supported);

    void setGlobalShortcutRegistered(bool registered);
    void setGlobalShortcutDescription(const QString& description);
    [[nodiscard]] QString stringWithShortcut(const QString& str) const;
    void actionWithShortcutToolTip(QAction* action) const;

    void setDefaultShortcut(const QKeySequence& key);
    void setDefaultShortcut(const ShortcutList& keys);
    [[nodiscard]] ShortcutList defaultShortcuts() const;
    [[nodiscard]] ShortcutList shortcuts() const;
    [[nodiscard]] ShortcutList globalShortcuts() const;
    [[nodiscard]] bool isGlobalShortcutRegistered() const;
    [[nodiscard]] QString globalShortcutDescription() const;
    [[nodiscard]] QKeySequence shortcut() const;

    void setDescription(const QString& text);
    [[nodiscard]] QString description() const;

    void setCategories(const QStringList& text);
    [[nodiscard]] QStringList categories() const;

    /*!
     * Returns the exclusive scope used for application shortcut conflict checking.
     * The default invalid scope overlaps all scopes. Global shortcut conflicts are unaffected.
     */
    [[nodiscard]] Id shortcutConflictScope() const;
    /*!
     * Sets an exclusive scope for application shortcut conflict checking.
     * Commands in different valid scopes may share application shortcuts. An invalid scope overlaps all scopes.
     * This metadata does not change action routing or global shortcut conflicts.
     */
    void setShortcutConflictScope(const Id& scope);

    void setCurrentContext(const Context& newContext);
    void addOverrideAction(QAction* actionToAdd, const Context& context, bool changeContext = true);
    bool removeOverrideAction(QAction* action, const Context& context);

Q_SIGNALS:
    void shortcutChanged();
    void globalShortcutsChanged();
    void globalShortcutRegistrationChanged();
    void globalShortcutDescriptionChanged();
    void activeStateChanged();
    void shortcutTargetsChanged();

private:
    std::unique_ptr<CommandPrivate> p;
};
} // namespace Fooyin
