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

#include "gui/fywidget.h"
#include "splitter.h"

class QHBoxLayout;

namespace Utils {
class ActionManager;
}

namespace Core {
class SettingsManager;
} // namespace Core

namespace Gui::Widgets {
class Dummy;
class WidgetProvider;

class SplitterWidget : public FyWidget
{
    Q_OBJECT

public:
    explicit SplitterWidget(Utils::ActionManager* actionManager, Widgets::WidgetProvider* widgetProvider,
                            Core::SettingsManager* settings, QWidget* parent = nullptr);

    void setupActions();

    [[nodiscard]] Qt::Orientation orientation() const;
    void setOrientation(Qt::Orientation orientation);

    [[nodiscard]] QByteArray saveState() const;
    bool restoreState(const QByteArray& state);

    [[nodiscard]] QWidget* widget(int index) const;
    [[nodiscard]] bool hasChildren();

    void addWidget(QWidget* widget);
    void replaceWidget(int index, FyWidget* widget);
    void replaceWidget(FyWidget* oldWidget, FyWidget* newWidget);
    void removeWidget(FyWidget* widget);

    int findIndex(FyWidget* widgetToFind);
    QList<FyWidget*> children();

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
    void layoutEditingMenu(Utils::ActionContainer* menu) override;
    void saveLayout(QJsonArray& array) override;
    void loadLayout(QJsonObject& object) override;

protected:
    void insertWidget(int index, FyWidget* widget);

private:
    Core::SettingsManager* m_settings;
    Utils::ActionManager* m_actionManager;
    Widgets::WidgetProvider* m_widgetProvider;

    QHBoxLayout* m_layout;
    Splitter* m_splitter;
    QList<FyWidget*> m_children;
    Dummy* m_dummy;
    QAction* m_changeSplitter;
    int m_widgetCount;
    bool m_isRoot;
};

class VerticalSplitterWidget : public SplitterWidget
{
public:
    explicit VerticalSplitterWidget(Utils::ActionManager* actionManager, Widgets::WidgetProvider* widgetProvider,
                                    Core::SettingsManager* settings, QWidget* parent = nullptr)
        : SplitterWidget(actionManager, widgetProvider, settings, parent)
    {
        setOrientation(Qt::Vertical);
    }
};

class HorizontalSplitterWidget : public SplitterWidget
{
public:
    explicit HorizontalSplitterWidget(Utils::ActionManager* actionManager, Widgets::WidgetProvider* widgetProvider,
                                      Core::SettingsManager* settings, QWidget* parent = nullptr)
        : SplitterWidget(actionManager, widgetProvider, settings, parent)
    {
        setOrientation(Qt::Horizontal);
    }
};
} // namespace Gui::Widgets
