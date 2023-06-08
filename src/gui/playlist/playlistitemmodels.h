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

#include <core/models/trackfwd.h>

namespace Fy::Gui::Widgets::Playlist {
class Container
{
public:
    Container();
    virtual ~Container() = default;

    [[nodiscard]] Core::TrackList tracks() const;
    [[nodiscard]] int trackCount() const;
    [[nodiscard]] uint64_t duration() const;

    virtual void addTrack(const Core::Track& track);
    virtual void removeTrack(const Core::Track& trackToRemove);

private:
    Core::TrackList m_tracks;
    uint64_t m_duration;
};
using ContainerHashMap = std::unordered_map<QString, std::unique_ptr<Container>>;

class Header : public Container
{
public:
    Header(QString title, QString subtitle, QString rightText, QString coverPath);

    [[nodiscard]] QString title() const;
    [[nodiscard]] QString subtitle() const;
    [[nodiscard]] QString rightText() const;
    [[nodiscard]] QString info() const;
    [[nodiscard]] bool hasCover() const;
    [[nodiscard]] QString coverPath() const;

    void addTrack(const Core::Track& track) override;
    void removeTrack(const Core::Track& trackToRemove) override;

private:
    QString m_title;
    QString m_subtitle;
    QString m_rightText;
    QStringList m_genres;
    QString m_coverPath;
};

class Subheader : public Container
{
public:
    Subheader(QString title);

    [[nodiscard]] QString title() const;

private:
    QString m_title;
};

class Track
{
public:
    Track() = default;
    Track(const Core::Track& track, QStringList leftSide, QStringList rightSide);

    [[nodiscard]] Core::Track track() const;
    [[nodiscard]] QStringList leftSide() const;
    [[nodiscard]] QStringList rightSide() const;

private:
    Core::Track m_track;
    QStringList m_leftSide;
    QStringList m_rightSide;
};
} // namespace Fy::Gui::Widgets::Playlist
