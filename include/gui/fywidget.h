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

#include "fygui_export.h"

#include <utils/id.h>

#include <QWidget>

#include <memory>
#include <unordered_map>

class QAction;
class QDialog;
class QMenu;

namespace Fooyin {
class FyWidgetPrivate;

class FYGUI_EXPORT LayoutCopyContext
{
public:
    /*!
     * Returns a copy-local replacement for @p value within @p scope.
     *
     * The first request for a scope/value pair creates a fresh stable value for
     * this copy operation; later requests for the same pair return the same
     * replacement. Widgets can use this to preserve relationships between copied
     * descendants while avoiding shared cross-instance state with the original
     * widgets.
     */
    [[nodiscard]] QString mappedString(const QString& scope, const QString& value);

private:
    std::unordered_map<QString, QString> m_stringMappings;
};

enum class EmptySearchMode : uint8_t
{
    Clear = 0,
    ShowAll,
};

struct SearchRequest
{
    QString text;
    EmptySearchMode emptyMode{EmptySearchMode::Clear};
};

/*!
 * Base class for all widgets that can participate in fooyin layouts.
 *
 * A widget must inherit FyWidget and be registered with WidgetProvider::registerWidget()
 * to be created by the layout system. Each instance is assigned a unique id at construction
 * time; this id can optionally be persisted in layout data and is also used by searchable
 * widgets to form connections with Search widgets.
 *
 * Reimplement @fn saveLayoutData() and @fn loadLayoutData() to persist widget-specific state.
 * For widgets that use the per-instance configuration model, these functions are expected to
 * serialise and restore the current instance config from the layout JSON.
 *
 * Reimplement @fn openConfigDialog() and use @fn addConfigureAction() to expose per-widget
 * configuration from a context menu. Use @fn showConfigDialog() to ensure only one config
 * dialog is open for a widget instance at a time.
 *
 * @note use WidgetContainer instead if your widget can contain other FyWidgets.
 *
 * @see WidgetProvider
 */
class FYGUI_EXPORT FyWidget : public QWidget
{
    Q_OBJECT

public:
    enum Feature : uint8_t
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
        /*!
         * Same as search, but this widget forms an exclusive connection to a search widget.
         */
        ExclusiveSearch = 1 << 2,
    };
    Q_DECLARE_FLAGS(Features, Feature)

    explicit FyWidget(QWidget* parent);
    ~FyWidget() override;

    [[nodiscard]] Id id() const;
    /*!
     * The name of this widget.
     * Used in layout editing menus, widget containers, and configuration dialogs.
     */
    [[nodiscard]] virtual QString name() const = 0;
    /*!
     * Stable type name written to layout files.
     * This should identify the widget type rather than a specific instance.
     */
    [[nodiscard]] virtual QString layoutName() const = 0;

    [[nodiscard]] Features features() const;
    [[nodiscard]] bool hasFeature(Feature feature) const;
    void setFeature(Feature feature, bool on = true);

    /** Returns the closest FyWidget parent of this widget. */
    [[nodiscard]] FyWidget* findParent() const;
    /** Returns the full geometry of this widget including frame. */
    [[nodiscard]] QRect widgetGeometry() const;

    /*!
     * Serialises this widget into the layout array.
     *
     * Persists the widget id when required by the active features and then calls
     * @fn saveLayoutData() for any widget-specific data.
     */
    void saveLayout(QJsonArray& layout);
    /*!
     * Serialises only the widget type and widget-specific data.
     *
     * Unlike @fn saveLayout(), this intentionally omits any persisted widget id.
     */
    void saveBaseLayout(QJsonArray& layout);
    /*!
     * Serialises this widget for layout copy/paste.
     *
     * This omits persisted ids and gives widgets a chance to rewrite
     * cross-instance state through @fn saveCopyLayoutData().
     */
    void saveCopyLayout(QJsonArray& layout, LayoutCopyContext& context, bool isRoot = true);
    /*!
     * Restores this widget from saved layout data.
     *
     * Replaces the widget id if one was saved and then calls @fn loadLayoutData()
     * for widget-specific restoration.
     */
    void loadLayout(const QJsonObject& layout);

    /*!
     * Receives a search term from a connected Search widget.
     *
     * In order to receive search events, enable Search or ExclusiveSearch using
     * @fn setFeature() and connect this widget instance to a Search widget.
     */
    virtual void searchEvent(const SearchRequest& request);

    /*!
     * Invoked when the widget context menu is built in layout editing mode.
     * Reimplement to add widget-specific actions or submenus.
     * @note the base class implementation of this function does nothing.
     */
    virtual void layoutEditingMenu(QMenu* menu);

    /*!
     * Called by @fn saveLayout() and @fn saveBaseLayout().
     * Reimplement to save widget-specific data to the layout object.
     * @note the base class implementation of this function does nothing.
     */
    virtual void saveLayoutData(QJsonObject& layout);
    /*!
     * Called by @fn saveCopyLayout().
     *
     * The base implementation writes normal widget data through @fn saveLayoutData()
     * and removes any persisted widget id.
     */
    virtual void saveCopyLayoutData(QJsonObject& layout, LayoutCopyContext& context, bool isRoot);
    /*!
     * Called by @fn loadLayout().
     * Reimplement to restore widget-specific data previously written by
     * @fn saveLayoutData().
     * @note the base class implementation of this function does nothing.
     */
    virtual void loadLayoutData(const QJsonObject& layout);

    /*!
     * Called after construction or after layout restoration has completed.
     *
     * Reimplement this if the widget needs to perform work that depends on its
     * saved layout data already being available.
     * @note the base class implementation of this function does nothing.
     */
    virtual void finalise();

protected:
    /*!
     * Opens this widget's configuration dialog.
     * Reimplement for widgets that support per-instance configuration.
     */
    virtual void openConfigDialog();
    /*!
     * Adds a standard `Configure…` action to @p menu and connects it to
     * @fn openConfigDialog().
     */
    QAction* addConfigureAction(QMenu* menu, bool addSeparator = true);
    /*!
     * Opens @p dialog and keeps it singleton per widget instance.
     * If a config dialog is already open for this widget, that dialog is focused instead.
     */
    void showConfigDialog(QDialog* dialog);

Q_SIGNALS:
    /*!
     * Sets the search term of the connected search widget.
     * @note this is only usable with the ExclusiveSearch feature.
     */
    void changeSearch(const QString& search);

private:
    std::unique_ptr<FyWidgetPrivate> p;
};
using WidgetList = std::vector<FyWidget*>;
} // namespace Fooyin

Q_DECLARE_OPERATORS_FOR_FLAGS(Fooyin::FyWidget::Features)
