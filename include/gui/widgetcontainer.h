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

#include <gui/fywidget.h>

class QPoint;

namespace Fooyin {
class SettingsManager;
class WidgetProvider;

/*!
 * Base class for layout widgets which hold other FyWidgets.
 *
 * Successful insertion transfers ownership to the container. Use takeWidget() or
 * exchangeWidget() to transfer live instances without deleting them.
 *
 * Reimplement saveLayoutData() to persist container configuration and child layouts,
 * preferably storing children under the 'Widgets' key. Editing state instead records
 * placement metadata for existing children without serialising or recreating them.
 */
class FYGUI_EXPORT WidgetContainer : public FyWidget
{
    Q_OBJECT

public:
    explicit WidgetContainer(WidgetProvider* widgetProvider, SettingsManager* settings, QWidget* parent = nullptr);

    [[nodiscard]] virtual bool canAddWidget() const                         = 0;
    [[nodiscard]] virtual bool canInsertWidget(int index) const             = 0;
    [[nodiscard]] virtual bool canMoveWidget(int index, int newIndex) const = 0;
    [[nodiscard]] virtual int widgetIndex(const Id& id) const               = 0;
    /*! Returns the immediate child with @p id, or nullptr when it is not present. */
    [[nodiscard]] virtual FyWidget* widgetAtId(const Id& id) const = 0;
    /*! Returns the immediate child at @p index, or nullptr for an invalid or empty index. */
    [[nodiscard]] virtual FyWidget* widgetAtIndex(int index) const = 0;
    /*!
     * Resolves @p pos in container coordinates to a layout widget.
     * Containers may return themselves for their own controls, such as a shared tab bar.
     * The default returns nullptr.
     */
    [[nodiscard]] virtual FyWidget* widgetAtPosition(const QPoint& pos) const;
    /*!
     * Returns the layout area representing @p widget in container coordinates.
     * The default maps the widget's actual geometry, or returns an empty rectangle for nullptr.
     */
    [[nodiscard]] virtual QRect widgetGeometry(FyWidget* widget) const;
    /*! Returns the immediate child count. Containers may exclude placeholder widgets. */
    [[nodiscard]] virtual int widgetCount() const = 0;
    /*!
     * Returns the child count used for positioning actions.
     * Defaults to widgetCount(); containers which need placeholder positions can include them here.
     */
    [[nodiscard]] virtual int fullWidgetCount() const;
    /*! Returns all immediate layout children in index order, including placeholders and hidden tabs. */
    [[nodiscard]] virtual WidgetList widgets() const = 0;

    /*! Returns the child arrangement direction. Defaults to Qt::Horizontal for containers without a split axis. */
    [[nodiscard]] virtual Qt::Orientation orientation() const;

    /*!
     * Adds @p widget at the container's default position and returns its index.
     * On success, the container takes ownership.
     */
    virtual int addWidget(FyWidget* widget) = 0;
    /*! Inserts @p widget at @p index, taking ownership on success. */
    virtual void insertWidget(int index, FyWidget* widget) = 0;
    /*!
     * Removes and schedules deletion of the child at @p index.
     * Use takeWidget() when the child must remain alive.
     */
    virtual void removeWidget(int index);
    /*! Calls exchangeWidget() and schedules deletion of the detached widget on success. */
    void replaceWidget(int index, FyWidget* newWidget);
    /*!
     * Replaces an existing widget with @p newWidget, preserving editing state and visibility.
     * Returns the hidden, parentless predecessor, whose ownership passes to the caller.
     * Returns nullptr on failure without changing the container or taking ownership of @p newWidget.
     */
    [[nodiscard]] FyWidget* exchangeWidget(int index, FyWidget* newWidget);
    /*! Reorders a child within this container. Call canMoveWidget() first. */
    virtual void moveWidget(int index, int newIndex) = 0;
    /*!
     * Detaches the child at @p index without deletion, replacement placeholders, or container cleanup.
     * Returns the hidden, parentless child, or nullptr for an invalid or empty index.
     * The caller owns the returned widget.
     */
    virtual FyWidget* takeWidget(int index) = 0;

    /*! Saves container state, such as splitter sizes. The default returns an empty byte array. */
    [[nodiscard]] virtual QByteArray saveState() const;
    /*! Restores saveState() data and reports success. The default is a no-op returning true. */
    virtual bool restoreState(const QByteArray& state);
    /*!
     * Saves placement metadata for the child at @p index, such as a tab title or splitter lock.
     * This does not save the child's configuration. The default returns an empty object.
     */
    [[nodiscard]] virtual QJsonObject saveChildState(int index) const;
    /*! Applies metadata to the existing child at @p index. The default does nothing. */
    virtual void restoreChildState(int index, const QJsonObject& state);
    /*!
     * Captures runtime placement state for layout editing, without saving child layouts.
     * The default combines saveState() with saveChildState() for each entry in widgets().
     * Overrides can add container-specific state, such as the active tab.
     */
    [[nodiscard]] virtual QJsonObject saveEditingState() const;
    /*!
     * Applies an editing snapshot to the current children by index.
     * Callers must restore the intended child order before applying the snapshot.
     * The default restores container state followed by metadata for the available child slots.
     */
    virtual void restoreEditingState(const QJsonObject& state);

    /*!
     * Saves copy-adjusted container data and child layouts under 'Widgets'.
     * The same @p context is shared with descendants to preserve relationships within the copy.
     */
    void saveCopyLayoutData(QJsonObject& layout, LayoutCopyContext& context, bool isRoot) override;

    /*!
     * Creates and adds children from saved layout objects, then finalises the added widgets.
     * Unavailable widget types are represented by placeholders which retain their saved data.
     * Existing children are not cleared; prepare the container before loading a replacement layout.
     */
    void loadWidgets(const QJsonArray& widgets);

protected:
    virtual FyWidget* exchangeWidgetImpl(int index, FyWidget* newWidget) = 0;

private:
    WidgetProvider* m_widgetProvider;
    SettingsManager* m_settings;
};
} // namespace Fooyin
