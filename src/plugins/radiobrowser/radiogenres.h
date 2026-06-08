/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include <QCoreApplication>
#include <QString>

#include <cstdint>
#include <vector>

namespace Fooyin::RadioBrowser {
enum class RadioGenreSection : uint8_t
{
    Genre = 0,
    MoreGenre,
    Era,
    Talk,
};

struct RadioGenre
{
    RadioGenreSection section{RadioGenreSection::Genre};
    QString name;
    QString tag;
};
using RadioGenreList = std::vector<RadioGenre>;

class RadioGenres
{
    Q_DECLARE_TR_FUNCTIONS(Fooyin::RadioBrowser::RadioGenres)

public:
    [[nodiscard]] static RadioGenreList all();
};
} // namespace Fooyin::RadioBrowser
