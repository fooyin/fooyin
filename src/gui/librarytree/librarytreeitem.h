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

#include <core/track.h>

#include <utils/treeitem.h>

#include <QObject>
#include <QString>

namespace Fooyin {
class LibraryTreeItem : public TreeItem<LibraryTreeItem>
{
public:
    enum Role
    {
        Title = Qt::UserRole,
        Level,
        Tracks,
    };

    LibraryTreeItem();
    explicit LibraryTreeItem(QString title, LibraryTreeItem* parent, int level);

    [[nodiscard]] bool pending() const;
    [[nodiscard]] int level() const;
    [[nodiscard]] QString title() const;
    [[nodiscard]] TrackList tracks() const;
    [[nodiscard]] int trackCount() const;
    [[nodiscard]] QString key() const;

    void setPending(bool pending);
    void setTitle(const QString& title);
    void setKey(const QString& key);

    void addTrack(const Track& track);
    void addTracks(const TrackList& tracks);
    void removeTrack(const Track& track);

    void sortChildren();

private:
    bool m_pending;
    int m_level;
    QString m_key;
    QString m_title;
    TrackList m_tracks;
};
} // namespace Fooyin
