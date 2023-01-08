/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include "libraryitem.h"

#include <QString>

namespace Core {
class Track;
class Container : public LibraryItem
{
public:
    Container() = default;
    explicit Container(QString title);
    ~Container() override;

    [[nodiscard]] QString title() const;

    [[nodiscard]] QString subTitle() const;
    void setSubTitle(const QString& title);

    [[nodiscard]] int trackCount() const;
    void setTrackCount(int count);

    [[nodiscard]] quint64 duration() const;
    void setDuration(quint64 duration);

    virtual void addTrack(Track* track);
    virtual void removeTrack(Track* track);
    virtual void reset();

private:
    QString m_title;
    QString m_subTitle;
    quint64 m_duration;
    int m_trackCount;
};
}; // namespace Core
