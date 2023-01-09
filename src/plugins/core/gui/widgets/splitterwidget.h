/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include "core/gui/fywidget.h"
#include "splitter.h"

class QHBoxLayout;

namespace Core {
class ActionManager;

namespace Widgets {
class Dummy;
class WidgetProvider;

class SplitterWidget : public FyWidget
{
    Q_OBJECT

public:
    explicit SplitterWidget(QWidget* parent = nullptr);
    ~SplitterWidget() override;

    void setupActions();

    [[nodiscard]] Qt::Orientation orientation() const;
    void setOrientation(Qt::Orientation orientation);

    [[nodiscard]] QByteArray saveState() const;
    bool restoreState(const QByteArray& state);

    [[nodiscard]] QWidget* widget(int index) const;
    [[nodiscard]] bool hasChildren();

    void addWidget(QWidget* widget);
    void insertWidget(int index, FyWidget* widget);
    void replaceWidget(int index, FyWidget* widget);
    void replaceWidget(FyWidget* oldWidget, FyWidget* newWidget);
    void removeWidget(FyWidget* widget);

    int findIndex(FyWidget* widgetToFind);
    QList<FyWidget*> children();

    [[nodiscard]] QString name() const override;
    void layoutEditingMenu(ActionContainer* menu) override;

    void saveSplitter(QJsonObject& object, QJsonArray& splitterArray);
    void loadSplitter(const QJsonArray& array, SplitterWidget* splitter);

private:
    QHBoxLayout* m_layout;
    Splitter* m_splitter;
    QList<FyWidget*> m_children;
    ActionManager* m_actionManager;
    Widgets::WidgetProvider* m_widgetProvider;
    Dummy* m_dummy;

    QAction* m_changeSplitter;
};

class VerticalSplitterWidget : public SplitterWidget
{
public:
    explicit VerticalSplitterWidget(QWidget* parent = nullptr)
        : SplitterWidget(parent)
    {
        setOrientation(Qt::Vertical);
    }
    ~VerticalSplitterWidget() override = default;
};

class HoriztonalSplitterWidget : public SplitterWidget
{
public:
    explicit HoriztonalSplitterWidget(QWidget* parent = nullptr)
        : SplitterWidget(parent)
    {
        setOrientation(Qt::Horizontal);
    }
    ~HoriztonalSplitterWidget() override = default;
};
}; // namespace Widgets
}; // namespace Core
