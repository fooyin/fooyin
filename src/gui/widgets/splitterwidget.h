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
class Dummy;

class SplitterWidget : public Widget,
                       public FactoryRegister<SplitterWidget>
{
    Q_OBJECT

public:
    explicit SplitterWidget(WidgetProvider* widgetProvider, QWidget* parent = nullptr);
    ~SplitterWidget() override;

    [[nodiscard]] Qt::Orientation orientation() const;
    void setOrientation(Qt::Orientation orientation);

    [[nodiscard]] QByteArray saveState() const;
    bool restoreState(const QByteArray& state);

    [[nodiscard]] QWidget* widget(int index) const;
    [[nodiscard]] bool hasChildren();

    void addWidget(QWidget* widget);
    void removeWidget(QWidget* widget);

    int findIndex(QWidget* widgetToFind);
    QList<Widget*> children();

    [[nodiscard]] QString name() const override;
    [[nodiscard]] static QString widgetName();
    void layoutEditingMenu(QMenu* menu) override;

    void saveSplitter(QJsonObject& object, QJsonArray& splitterArray);
    void loadSplitter(const QJsonArray& array, SplitterWidget* splitter);

private:
    QHBoxLayout* m_layout;
    Splitter* m_splitter;
    QList<Widget*> m_children;
    WidgetProvider* m_widgetProvider;
    Dummy* m_dummy;
};
