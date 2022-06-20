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

#include "splitter.h"
#include "widget.h"

class WidgetProvider;
class QHBoxLayout;

class SplitterWidget : public Widget
{
    Q_OBJECT

public:
    SplitterWidget(Qt::Orientation type, WidgetProvider* widgetProvider, QWidget* parent = nullptr);
    ~SplitterWidget() override;

    [[nodiscard]] Qt::Orientation orientation() const;
    void setOrientation(Qt::Orientation orientation);

    [[nodiscard]] QByteArray saveState() const;
    bool restoreState(const QByteArray& state);

    [[nodiscard]] QWidget* widget(int index) const;

    void addToSplitter(Widgets::WidgetType type, QWidget* widget);
    void removeWidget(QWidget* widget);

    int findIndex(QWidget* widgetToFind);
    int findIndex(Widgets::WidgetType typeToFind);
    QList<SplitterEntry> children();

    void layoutEditingMenu(QMenu* menu) override;

protected:
    [[nodiscard]] int placeholderIndex() const;

private:
    QHBoxLayout* m_layout;
    Splitter* m_splitter;
    QList<SplitterEntry> m_children;
    WidgetProvider* m_widgetProvider;
};
