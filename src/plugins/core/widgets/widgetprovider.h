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

#include "core/typedefs.h"

#include <QObject>

namespace Library {
class LibraryManager;
class MusicLibrary;
} // namespace Library

class PlayerManager;
class SettingsDialog;
class SplitterWidget;
class Widget;
class QMenu;

class WidgetProvider : public QObject
{
    Q_OBJECT

public:
    WidgetProvider(PlayerManager* playerManager, Library::LibraryManager* libraryManager,
                   Library::MusicLibrary* library, SettingsDialog* settingsDialog, QObject* parent = nullptr);
    ~WidgetProvider() override;

    Widget* createWidget(const QString& widget, SplitterWidget* splitter);
    //    Widget* createWidget(Widgets::WidgetType type, SplitterWidget* splitter);
    Widget* createFilter(Filters::FilterType filterType, SplitterWidget* splitter);
    static SplitterWidget* createSplitter(Qt::Orientation type, QWidget* parent);

    void addMenuActions(QMenu* menu, SplitterWidget* splitter);
    //    void addWidgetMenu(QMenu* menu, SplitterWidget* splitter);
    //    void addFilterMenu(QMenu* menu, SplitterWidget* splitter);

protected:
    static void registerWidgets();

private:
    struct Private;
    std::unique_ptr<WidgetProvider::Private> p;
};
