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

#include <core/track.h>

#include <utils/treeitem.h>

#include <QObject>
#include <QString>

namespace Fy::Gui::Widgets {
enum LibraryTreeRole
{
    Title = Qt::UserRole,
    Level,
    Tracks,
};

class LibraryTreeItem : public Utils::TreeItem<LibraryTreeItem>
{
public:
    LibraryTreeItem();
    explicit LibraryTreeItem(QString title, LibraryTreeItem* parent, int level);

    [[nodiscard]] bool pending() const;
    [[nodiscard]] int level() const;
    [[nodiscard]] QString title() const;
    [[nodiscard]] Core::TrackList tracks() const;
    [[nodiscard]] int trackCount() const;
    [[nodiscard]] QString key() const;

    void setPending(bool pending);
    void setTitle(const QString& title);
    void setKey(const QString& key);

    void addTrack(const Core::Track& track);
    void addTracks(const Core::TrackList& tracks);
    void removeTrack(const Core::Track& track);

    void sortChildren();

private:
    bool m_pending;
    int m_level;
    QString m_key;
    QString m_title;
    Core::TrackList m_tracks;
};
} // namespace Fy::Gui::Widgets
