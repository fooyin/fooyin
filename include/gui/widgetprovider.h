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
class QMenu;

namespace Fooyin {
class EditableLayout;
class Id;
class FyWidget;
class WidgetContainer;
class WidgetProviderPrivate;

/*!
 * Handles registration of FyWidgets.
 */
class FYGUI_EXPORT WidgetProvider
{
public:
    WidgetProvider();
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
    bool registerWidget(const QString& key, std::function<FyWidget*()> instantiator, const QString& displayName);

    /*!
     * Sets the submenus the widget at @p key appears at in add/replace menus when layout editing.
     * The widget must already be registered using @fn registerWidget with the same @p key.
     */
    void setSubMenus(const QString& key, const QStringList& subMenus);

    /** Sets the maximum number of instances which can be created of the widget at @p key. */
    void setLimit(const QString& key, int limit);
    /** Sets whether the widget at @p key is shown in layout editing menus. */
    void setIsHidden(const QString& key, bool hidden);

    /** Returns @c true if the widget at @p key exists. */
    [[nodiscard]] bool widgetExists(const QString& key) const;
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
     * @param index the index to insert at.
     */
    void setupAddWidgetMenu(EditableLayout* layout, QMenu* menu, WidgetContainer* container, int index);
    /*!
     * Fills the passed @p menu with actions to replace a widget with an instance of another registered widget.
     * @param menu the menu to add actions to.
     * @param container the container in which widgets will be replaced.
     * @param widgetId the widget to replace
     */
    void setupReplaceWidgetMenu(EditableLayout* layout, QMenu* menu, WidgetContainer* container, const Id& widgetId);
    /*!
     * Fills the passed @p menu with actions to split a widget.
     * @param menu the menu to add actions to.
     * @param container the container in which widgets will be replaced.
     * @param widgetId the widget to replace
     */
    void setupSplitWidgetMenu(EditableLayout* layout, QMenu* menu, WidgetContainer* container, const Id& widgetId);

private:
    std::unique_ptr<WidgetProviderPrivate> p;
};
} // namespace Fooyin
