/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <LukeT1@proton.me>
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

#include <gui/widgetcontainer.h>

#include <QPointer>

namespace Fooyin {
class SettingsManager;
class Splitter;
class WidgetProvider;

class SplitterWidget : public WidgetContainer
{
    Q_OBJECT

public:
    SplitterWidget(WidgetProvider* widgetProvider, SettingsManager* settings, QWidget* parent = nullptr);
    ~SplitterWidget() override;

    [[nodiscard]] Qt::Orientation orientation() const override;
    void setOrientation(Qt::Orientation orientation);

    [[nodiscard]] QByteArray saveState() const override;
    bool restoreState(const QByteArray& state) override;

    [[nodiscard]] bool canAddWidget() const override;
    [[nodiscard]] bool canMoveWidget(int index, int newIndex) const override;
    [[nodiscard]] int widgetIndex(const Id& id) const override;
    [[nodiscard]] FyWidget* widgetAtId(const Id& id) const override;
    [[nodiscard]] FyWidget* widgetAtIndex(int index) const override;
    [[nodiscard]] int widgetCount() const override;
    [[nodiscard]] int fullWidgetCount() const override;
    [[nodiscard]] WidgetList widgets() const override;

    int addWidget(FyWidget* widget) override;
    void insertWidget(int index, FyWidget* widget) override;
    void removeWidget(int index) override;
    void replaceWidget(int index, FyWidget* newWidget) override;
    void moveWidget(int index, int newIndex) override;

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
    void layoutEditingMenu(QMenu* menu) override;
    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;
    void finalise() override;

private:
    WidgetProvider* m_widgetProvider;

    Splitter* m_splitter;
    WidgetList m_widgets;
};

class VerticalSplitterWidget : public SplitterWidget
{
public:
    explicit VerticalSplitterWidget(WidgetProvider* widgetFactory, SettingsManager* settings, QWidget* parent = nullptr)
        : SplitterWidget(widgetFactory, settings, parent)
    {
        setOrientation(Qt::Vertical);
    }
};

class HorizontalSplitterWidget : public SplitterWidget
{
public:
    explicit HorizontalSplitterWidget(WidgetProvider* widgetFactory, SettingsManager* settings,
                                      QWidget* parent = nullptr)
        : SplitterWidget(widgetFactory, settings, parent)
    {
        setOrientation(Qt::Horizontal);
    }
};
} // namespace Fooyin
