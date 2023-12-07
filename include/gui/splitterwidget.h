/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include <gui/widgetcontainer.h>

namespace Fooyin {
class ActionManager;
class SettingsManager;
class WidgetProvider;

class FYGUI_EXPORT SplitterWidget : public WidgetContainer
{
    Q_OBJECT

public:
    SplitterWidget(ActionManager* actionManager, WidgetProvider* widgetProvider, SettingsManager* settings,
                   QWidget* parent = nullptr);
    ~SplitterWidget() override;

    void setWidgetLimit(int count);
    void showPlaceholder(bool show);

    [[nodiscard]] Qt::Orientation orientation() const;
    void setOrientation(Qt::Orientation orientation);

    [[nodiscard]] QByteArray saveState() const;
    bool restoreState(const QByteArray& state);

    [[nodiscard]] int childCount();

    void addWidget(FyWidget* widget) override;
    void replaceWidget(FyWidget* oldWidget, FyWidget* newWidget) override;
    void removeWidget(FyWidget* widget) override;

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
    void layoutEditingMenu(ActionContainer* menu) override;
    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;

private:
    struct Private;
    std::unique_ptr<Private> p;
};

class FYGUI_EXPORT VerticalSplitterWidget : public SplitterWidget
{
public:
    explicit VerticalSplitterWidget(ActionManager* actionManager, WidgetProvider* widgetFactory,
                                    SettingsManager* settings, QWidget* parent = nullptr)
        : SplitterWidget(actionManager, widgetFactory, settings, parent)
    {
        setOrientation(Qt::Vertical);
    }
};

class FYGUI_EXPORT HorizontalSplitterWidget : public SplitterWidget
{
public:
    explicit HorizontalSplitterWidget(ActionManager* actionManager, WidgetProvider* widgetFactory,
                                      SettingsManager* settings, QWidget* parent = nullptr)
        : SplitterWidget(actionManager, widgetFactory, settings, parent)
    {
        setOrientation(Qt::Horizontal);
    }
};
} // namespace Fooyin
