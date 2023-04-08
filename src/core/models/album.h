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

#include <QList>

namespace Fy::Core {
class Track;

class Album
{
public:
    Album();
    explicit Album(QString title);

    [[nodiscard]] QString title() const;
    [[nodiscard]] QString subTitle() const;
    [[nodiscard]] QString artist() const;
    [[nodiscard]] QString date() const;
    [[nodiscard]] QStringList genres() const;
    [[nodiscard]] bool hasCover() const;
    [[nodiscard]] QString coverPath() const;

    [[nodiscard]] bool isSingleDiscAlbum() const;

    [[nodiscard]] int trackCount() const;
    [[nodiscard]] uint64_t duration() const;

    void setTitle(const QString& title);
    void setSubTitle(const QString& title);
    void setArtist(const QString& artist);
    void setDate(const QString& date);
    void setGenres(const QStringList& genres);
    void setDiscCount(int count);
    void setCoverPath(const QString& path);

    void addTrack(Track* track);
    void removeTrack(Track* track);

    void reset();

private:
    QString m_title;
    QString m_subTitle;
    QString m_artist;
    QString m_date;
    QStringList m_genres;
    QString m_coverPath;
    uint64_t m_duration;
    int m_trackCount;
    int m_discCount;
};
using AlbumHash = std::unordered_map<QString, Album>;
} // namespace Fy::Core
