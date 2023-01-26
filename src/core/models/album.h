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

#include <QString>
#include <QStringList>

namespace Core {
class Track;
class Album : public Container
{
public:
    Album() = default;
    explicit Album(const QString& title);
    ~Album() override = default;

    [[nodiscard]] int id() const;
    void setId(int id);

    [[nodiscard]] int artistId() const;
    void setArtistId(int id);

    [[nodiscard]] QString artist() const;
    void setArtist(const QString& artist);

    [[nodiscard]] int year() const;
    void setYear(int year);

    [[nodiscard]] QStringList genres() const;
    void setGenres(const QStringList& genres);

    [[nodiscard]] QString mainGenre() const;

    [[nodiscard]] int discCount() const;
    void setDiscCount(int count);

    [[nodiscard]] bool isSingleDiscAlbum() const;

    [[nodiscard]] bool hasCover() const;

    [[nodiscard]] QString coverPath() const;
    void setCoverPath(const QString& path);

    void addTrack(Track* track) override;
    void removeTrack(Track* track) override;
    void reset() override;

private:
    int m_id;
    int m_artistId;
    QString m_artist;
    int m_year;
    QStringList m_genres;
    int m_discCount;
    QString m_coverPath;
};
} // namespace Core
