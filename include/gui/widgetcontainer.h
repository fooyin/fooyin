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

#include <gui/fywidget.h>

namespace Fooyin {
class SettingsManager;
class WidgetProvider;

/*!
 * Represents a container of FyWidgets.
 * This should be used as the base class for FyWidgets which can contain/hold other
 * FyWidgets.
 * It's recommended to save the child widgets under the 'Widgets' key
 * when reimplementing @fn saveLayoutData.
 */
class WidgetContainer : public FyWidget
{
    Q_OBJECT

public:
    explicit WidgetContainer(WidgetProvider* widgetProvider, SettingsManager* settings, QWidget* parent = nullptr);

    [[nodiscard]] virtual bool canAddWidget() const                         = 0;
    [[nodiscard]] virtual bool canMoveWidget(int index, int newIndex) const = 0;
    [[nodiscard]] virtual int widgetIndex(const Id& id) const               = 0;
    [[nodiscard]] virtual FyWidget* widgetAtId(const Id& id) const          = 0;
    [[nodiscard]] virtual FyWidget* widgetAtIndex(int index) const          = 0;
    [[nodiscard]] virtual int widgetCount() const                           = 0;
    [[nodiscard]] virtual int fullWidgetCount() const;
    [[nodiscard]] virtual WidgetList widgets() const = 0;

    [[nodiscard]] virtual Qt::Orientation orientation() const;

    virtual int addWidget(FyWidget* widget)                    = 0;
    virtual void insertWidget(int index, FyWidget* widget)     = 0;
    virtual void removeWidget(int index)                       = 0;
    virtual void replaceWidget(int index, FyWidget* newWidget) = 0;
    virtual void moveWidget(int index, int newIndex)           = 0;

    [[nodiscard]] virtual QByteArray saveState() const;
    virtual bool restoreState(const QByteArray& state);

    /*!
     * Convenience method to load all widgets in the @p widgets array.
     */
    void loadWidgets(const QJsonArray& widgets);

private:
    WidgetProvider* m_widgetProvider;
    SettingsManager* m_settings;
};
} // namespace Fooyin
