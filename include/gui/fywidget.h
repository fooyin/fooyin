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

#include <utils/id.h>

#include <QWidget>

class QMenu;

namespace Fooyin {
/*!
 * Base class for all widgets in fooyin.
 * A widget must be a subclass of FyWidget and registered using WidgetProvider::registerWidget
 * to be used with the layout system.
 * A unique id is generated for each widget instance. This is saved to the layout
 * to be restored on load. Reimplement @fn saveLayoutData and @fn loadLayoutData to
 * save/restore any extra widget data.
 * @note use WidgetContainer instead if your widget can contain other FyWidgets.
 *
 * @see WidgetProvider
 */
class FYGUI_EXPORT FyWidget : public QWidget
{
    Q_OBJECT

public:
    enum Feature
    {
        None = 0,
        /*!
         * Persist the unique id of this widget in the layout.
         * @note this is disabled by default.
         * @note this is ignored for searchable widgets.
         */
        PersistId = 1 << 0,
        /*!
         * Mark this widget searchable. Needed to form connections with search widgets.
         * Reimplement @fn searchEvent to receive search events.
         */
        Search = 1 << 1,
    };
    Q_DECLARE_FLAGS(Features, Feature)

    explicit FyWidget(QWidget* parent);

    [[nodiscard]] Id id() const;
    /*!
     * The name of this widget.
     * Used in layout editing menus and widget containers.
     */
    [[nodiscard]] virtual QString name() const = 0;
    /*!
     * Used when saving to a layout file.
     */
    [[nodiscard]] virtual QString layoutName() const = 0;

    [[nodiscard]] Features features() const;
    void setFeature(Feature feature, bool on = true);

    /** Returns the closest FyWidget parent of this widget. */
    [[nodiscard]] FyWidget* findParent() const;
    /** Returns the full geometry of this widget including frame. */
    [[nodiscard]] QRect widgetGeometry() const;

    /*!
     * Called when saving to a layout file (on close or exporting).
     * Saves the unique id of this widget and calls @fn saveLayoutData.
     */
    void saveLayout(QJsonArray& layout);
    /*!
     * Saves base widget data excluding any unique identifiers.
     */
    void saveBaseLayout(QJsonArray& layout);
    /*!
     * Called when loading from a layout file (on open or importing).
     * Replaces the unique id of this widget (if it exists) and calls @fn loadLayoutData.
     */
    void loadLayout(const QJsonObject& layout);

    /*!
     * A search event is sent to a widget from it's connected search widget.
     * In order to receive search events, add the Search feature using @fn setFeature
     * and connect an instance of this widget to a search widget.
     */
    virtual void searchEvent(const QString& search);

    /*!
     * Called when opening the context menu for this widget in layout editing mode.
     * Reimplement to add additional actions/menus.
     * @note the base class implementation of this function does nothing.
     */
    virtual void layoutEditingMenu(QMenu* menu);

    /*!
     * Called when saving layout in @fn saveLayout.
     * Reimplement to save additional widget data to the layout file.
     * @note the base class implementation of this function does nothing.
     */
    virtual void saveLayoutData(QJsonObject& layout);
    /*!
     * Called when loading layout in @fn loadLayout.
     * Reimplement to load additional widget data previously saved to the layout file
     * in @fn saveLayoutData.
     * @note the base class implementation of this function does nothing.
     */
    virtual void loadLayoutData(const QJsonObject& layout);

    /*!
     * Called either immediately after this widget has been instantiated or after
     * it's data has been loaded from layout.
     * @note the base class implementation of this function does nothing.
     */
    virtual void finalise();

private:
    Id m_id;
    Features m_features;
};
using WidgetList = std::vector<FyWidget*>;
} // namespace Fooyin

Q_DECLARE_OPERATORS_FOR_FLAGS(Fooyin::FyWidget::Features)
