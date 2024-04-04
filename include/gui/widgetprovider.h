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

#include "fygui_export.h"

#include <QString>

#include <functional>
#include <memory>

class QUndoStack;

namespace Fooyin {
class ActionManager;
class ActionContainer;
class Id;
class FyWidget;
class WidgetContainer;
class WidgetFactory;

/*!
 * Handles registration of FyWidgets.
 */
class FYGUI_EXPORT WidgetProvider
{
public:
    explicit WidgetProvider(ActionManager* actionManager);
    ~WidgetProvider();

    void setCommandStack(QUndoStack* layoutCommands);

    /*!
     * Registers a widget.
     * FyWidget subclasses must be registered to be used with the layout system.
     * @param key a unique key to associate with the widget.
     * @param instantiator a function to instantiate a FyWidget subclass.
     * @param displayName name to use in layout editing menus.
     * @returns true if the widget was registered, or false if a widget at @p key already exists.
     */
    bool registerWidget(const QString& key, std::function<FyWidget*()> instantiator,
                        const QString& displayName = QStringLiteral(""));

    /*!
     * Sets the submenus the widget at @p key appears at in add/replace menus when layout editing.
     * The widget must already be registerd using @fn registerWidget with the same @p key.
     */
    void setSubMenus(const QString& key, const QStringList& subMenus);

    /** Sets the maximum number of instances which can be created of the widget at @p key. */
    void setLimit(const QString& key, int limit);

    /** Returns @c true if an instance can be created of the widget at @p key. */
    [[nodiscard]] bool canCreateWidget(const QString& key) const;

    /*!
     * Creates the widget associated with the @p key.
     * @returns the new widget instance, or nullptr if not registered or over the limit for this widget.
     */
    FyWidget* createWidget(const QString& key);

    /*!
     * Fills the passed @p menu with actions to create a new instance of each registered widget.
     * @param menu the menu to add actions to.
     * @param container the container in which widgets will be added.
     */
    void setupAddWidgetMenu(ActionContainer* menu, WidgetContainer* container);
    /*!
     * Fills the passed @p menu with actions to replace a widget with an instance of another registered widget.
     * @param menu the menu to add actions to.
     * @param container the container in which widgets will be replaced.
     * @param widgetId the widget to replace
     */
    void setupReplaceWidgetMenu(ActionContainer* menu, WidgetContainer* container, const Id& widgetId);

    void updateActionState();

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin
