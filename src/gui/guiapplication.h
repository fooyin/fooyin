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

#include "fygui_export.h"

#include <core/track.h>

#include <QObject>

#include <set>

namespace Fooyin {
class ActionManager;
class Application;
class EditableLayout;
class GuiApplicationPrivate;
class LayoutProvider;
class PlaylistController;
class PropertiesDialog;
class SearchController;
class ThemeRegistry;
class TrackSelectionController;
class WidgetProvider;

class FYGUI_EXPORT GuiApplication : public QObject
{
    Q_OBJECT

public:
    explicit GuiApplication(Application* core);
    ~GuiApplication() override;

    void startup();
    void shutdown();

    void raise();
    void openFiles(const QList<QUrl>& files);

    void searchForArtwork(const TrackList& tracks, Track::Cover type, bool quick);
    void removeArtwork(const TrackList& tracks, const std::set<Track::Cover>& types);
    void removeAllArtwork(const TrackList& tracks);

    [[nodiscard]] ActionManager* actionManager() const;
    [[nodiscard]] LayoutProvider* layoutProvider() const;
    [[nodiscard]] PlaylistController* playlistController() const;
    [[nodiscard]] TrackSelectionController* trackSelection() const;
    [[nodiscard]] SearchController* searchController() const;
    [[nodiscard]] PropertiesDialog* propertiesDialog() const;
    [[nodiscard]] EditableLayout* editableLayout() const;
    [[nodiscard]] WidgetProvider* widgetProvider() const;
    [[nodiscard]] ThemeRegistry* themeRegistry() const;

private:
    std::unique_ptr<GuiApplicationPrivate> p;
};
} // namespace Fooyin
