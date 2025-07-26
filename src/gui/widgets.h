/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include <core/track.h>

#include <QObject>

namespace Fooyin {
class Application;
class CoverProvider;
class FyWidget;
class GuiApplication;
class LibraryTreeController;
class MainWindow;
class PlaylistController;
class PlaylistInteractor;
struct ScanProgress;
class StatusWidget;
class SettingsManager;
class ThemeRegistry;
class WidgetProvider;

class Widgets : public QObject
{
    Q_OBJECT

public:
    Widgets(Application* core, MainWindow* window, GuiApplication* gui, PlaylistInteractor* playlistInteractor,
            QObject* parent = nullptr);

    void registerWidgets();
    void registerPages();
    void registerPropertiesTabs();
    void registerFontEntries() const;

    void showArtworkDialog(const TrackList& tracks, Track::Cover type, bool quick);

private:
    FyWidget* createDirBrowser();
    static void showScanProgress(const ScanProgress& progress);

    Application* m_core;
    GuiApplication* m_gui;

    MainWindow* m_window;
    SettingsManager* m_settings;

    CoverProvider* m_coverProvider;
    PlaylistInteractor* m_playlistInteractor;
    PlaylistController* m_playlistController;
    LibraryTreeController* m_libraryTreeController;
};
} // namespace Fooyin
