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

#include "utils/typedefs.h"

#include <QMainWindow>
#include <QWidget>

class QHBoxLayout;
class Overlay;
class SplitterWidget;
class WidgetProvider;
class Widget;
class Settings;

class EditableLayout : public QWidget
{
    Q_OBJECT

public:
    explicit EditableLayout(WidgetProvider* widgetProvider, QWidget* parent = nullptr);
    ~EditableLayout() override;

    void changeLayout(const QByteArray& layout);

    static Widget* splitterChild(QWidget* widget);

    bool eventFilter(QObject* watched, QEvent* event) override;

    //    void saveSplitter(QJsonObject& object, QJsonArray& array, SplitterWidget* splitter, bool isRoot);
    void saveLayout();
    //    void loadSplitter(const QJsonArray& array, SplitterWidget* splitter);
    bool loadLayout(const QByteArray& layout);
    bool loadLayout();

    static QRect widgetGeometry(Widget* widget);
    void showOverlay(Widget* widget);
    void hideOverlay();

private:
    QHBoxLayout* m_box;
    Settings* m_settings;
    bool m_layoutEditing;
    Overlay* m_overlay;
    SplitterWidget* m_splitter;
    QMenu* m_menu;
    WidgetProvider* m_widgetProvider;
};
