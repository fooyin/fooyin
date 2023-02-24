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

#include "container.h"

#include <QList>

namespace Core {
class Track;

using GenreList = QList<QString>;

class Album : public Container
{
public:
    Album() = default;
    explicit Album(const QString& title);

    [[nodiscard]] int id() const;
    [[nodiscard]] int artistId() const;
    [[nodiscard]] QString artist() const;
    [[nodiscard]] QString date() const;
    [[nodiscard]] int year() const;
    [[nodiscard]] GenreList genres() const;
    [[nodiscard]] int discCount() const;
    [[nodiscard]] bool isSingleDiscAlbum() const;
    [[nodiscard]] bool hasCover() const;
    [[nodiscard]] QString coverPath() const;

    void setId(int id);
    void setArtistId(int id);
    void setArtist(const QString& artist);
    void setDate(const QString& date);
    void setGenres(const GenreList& genres);
    void setDiscCount(int count);
    void setCoverPath(const QString& path);

    void addTrack(Track* track) override;
    void removeTrack(Track* track) override;

    void reset() override;

private:
    int m_id;
    int m_artistId;
    QString m_artist;
    QString m_date;
    int m_year;
    GenreList m_genres;
    int m_discCount;
    QString m_coverPath;
};
} // namespace Core
