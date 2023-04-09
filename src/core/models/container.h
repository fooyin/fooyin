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

#include <QString>

namespace Fy::Core {
class Track;
class Container
{
public:
    Container() = default;
    explicit Container(QString title);

    [[nodiscard]] QString title() const;
    [[nodiscard]] QString subTitle() const;
    [[nodiscard]] int trackCount() const;
    [[nodiscard]] uint64_t duration() const;

    void setSubTitle(const QString& title);
    void setTrackCount(int count);
    void setDuration(uint64_t duration);

    void addTrack(const Track& track);
    void removeTrack(const Track& track);
    void reset();

private:
    QString m_title;
    QString m_subTitle;
    uint64_t m_duration;
    int m_trackCount;
};
using ContainerHash = std::unordered_map<QString, Container>;
} // namespace Fy::Core
