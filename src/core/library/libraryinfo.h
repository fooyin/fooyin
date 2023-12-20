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

#include "fycore_export.h"

#include <QObject>

#include <map>

namespace Fooyin {
struct FYCORE_EXPORT LibraryInfo
{
    Q_GADGET

public:
    enum class Status
    {
        Idle,
        Pending,
        Initialised,
        Scanning,
    };
    Q_ENUM(Status)

    LibraryInfo() = default;
    LibraryInfo(QString name, QString path, int id = -1)
        : name{std::move(name)}
        , path{std::move(path)}
        , id{id} {};
    QString name;
    QString path;
    int id{-1};
    Status status{Status::Idle};
};
using LibraryInfoMap = std::map<int, LibraryInfo>;
} // namespace Fooyin
