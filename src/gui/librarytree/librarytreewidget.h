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

class QTreeView;
class QVBoxLayout;

namespace Fy {
namespace Utils {
class SettingsManager;
} // namespace Utils

namespace Core {
namespace Library {
class MusicLibrary;
} // namespace Library

namespace Playlist {
class PlaylistHandler;
} // namespace Playlist
} // namespace Core

namespace Gui::Widgets {
class LibraryTreeModel;

class LibraryTreeWidget : public FyWidget
{
public:
    LibraryTreeWidget(Core::Library::MusicLibrary* library, Core::Playlist::PlaylistHandler* playlistHandler, Utils::SettingsManager* settings,
                      QWidget* parent = nullptr);

    QString name() const override;
    QString layoutName() const override;

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void groupingChanged(const QString& script);
    void reset();

    Core::Library::MusicLibrary* m_library;
    Core::Playlist::PlaylistHandler* m_playlistHandler;
    Utils::SettingsManager* m_settings;

    QVBoxLayout* m_layout;
    QTreeView* m_trackTree;
    LibraryTreeModel* m_model;
};
} // namespace Gui::Widgets
} // namespace Fy
