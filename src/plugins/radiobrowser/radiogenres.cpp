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

#include "radiogenres.h"

using namespace Qt::StringLiterals;

namespace Fooyin::RadioBrowser {
RadioGenreList RadioGenres::all()
{
    return {
        {RadioGenreSection::Genre, tr("Blues"), u"blues"_s},
        {RadioGenreSection::Genre, tr("Classical"), u"classical"_s},
        {RadioGenreSection::Genre, tr("Country"), u"country"_s},
        {RadioGenreSection::Genre, tr("Dance"), u"dance"_s},
        {RadioGenreSection::Genre, tr("Disco"), u"disco"_s},
        {RadioGenreSection::Genre, tr("Easy"), u"easy listening"_s},
        {RadioGenreSection::Genre, tr("Folk"), u"folk"_s},
        {RadioGenreSection::Genre, tr("Hits"), u"hits"_s},
        {RadioGenreSection::Genre, tr("Jazz"), u"jazz"_s},
        {RadioGenreSection::Genre, tr("Oldies"), u"oldies"_s},
        {RadioGenreSection::Genre, tr("Pop"), u"pop"_s},
        {RadioGenreSection::Genre, tr("Rap"), u"rap"_s},
        {RadioGenreSection::Genre, tr("Rock"), u"rock"_s},
        {RadioGenreSection::Genre, tr("Soul"), u"soul"_s},

        {RadioGenreSection::MoreGenre, tr("Alternative"), u"alternative"_s},
        {RadioGenreSection::MoreGenre, tr("Ambient"), u"ambient"_s},
        {RadioGenreSection::MoreGenre, tr("Club"), u"club"_s},
        {RadioGenreSection::MoreGenre, tr("Electronic"), u"electronic"_s},
        {RadioGenreSection::MoreGenre, tr("Funk"), u"funk"_s},
        {RadioGenreSection::MoreGenre, tr("Hip Hop"), u"hiphop"_s},
        {RadioGenreSection::MoreGenre, tr("House"), u"house"_s},
        {RadioGenreSection::MoreGenre, tr("Indie"), u"indie"_s},
        {RadioGenreSection::MoreGenre, tr("Latino"), u"latino"_s},
        {RadioGenreSection::MoreGenre, tr("Metal"), u"metal"_s},
        {RadioGenreSection::MoreGenre, tr("Punk"), u"punk"_s},
        {RadioGenreSection::MoreGenre, tr("Reggae"), u"reggae"_s},
        {RadioGenreSection::MoreGenre, tr("Salsa"), u"salsa"_s},
        {RadioGenreSection::MoreGenre, tr("World Music"), u"world music"_s},

        {RadioGenreSection::Era, tr("40s"), u"40s"_s},
        {RadioGenreSection::Era, tr("50s"), u"50s"_s},
        {RadioGenreSection::Era, tr("60s"), u"60s"_s},
        {RadioGenreSection::Era, tr("70s"), u"70s"_s},
        {RadioGenreSection::Era, tr("80s"), u"80s"_s},
        {RadioGenreSection::Era, tr("90s"), u"90s"_s},
        {RadioGenreSection::Era, tr("2000s"), u"2000s"_s},
        {RadioGenreSection::Era, tr("2010s"), u"2010s"_s},
        {RadioGenreSection::Era, tr("Contemporary"), u"contemporary"_s},

        {RadioGenreSection::Talk, tr("AM"), u"am"_s},
        {RadioGenreSection::Talk, tr("Comedy"), u"comedy"_s},
        {RadioGenreSection::Talk, tr("College Radio"), u"college radio"_s},
        {RadioGenreSection::Talk, tr("Community Radio"), u"community radio"_s},
        {RadioGenreSection::Talk, tr("Culture"), u"culture"_s},
        {RadioGenreSection::Talk, tr("Educational"), u"educational"_s},
        {RadioGenreSection::Talk, tr("Kids"), u"kids"_s},
        {RadioGenreSection::Talk, tr("News"), u"news"_s},
        {RadioGenreSection::Talk, tr("Public Radio"), u"public radio"_s},
        {RadioGenreSection::Talk, tr("Religion"), u"religion"_s},
        {RadioGenreSection::Talk, tr("Sport"), u"sport"_s},
        {RadioGenreSection::Talk, tr("Talk"), u"talk"_s},
    };
}
} // namespace Fooyin::RadioBrowser
