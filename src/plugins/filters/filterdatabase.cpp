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

#include "filterdatabase.h"

#include <QBuffer>
#include <QPixmap>
#include <core/database/query.h>
#include <utils/helpers.h>
#include <utils/utils.h>

namespace {

QString getFilterJoins(const Filters::FilterType type = {})
{
    QString joins;
    switch(type) {
        case(Filters::FilterType::AlbumArtist):
            joins += "LEFT JOIN Artists AS AlbumArtists ON AlbumArtists.ArtistID = "
                     "Tracks.AlbumArtistID ";
            break;
        case(Filters::FilterType::Artist):
            joins += "LEFT JOIN TrackArtists ON TrackArtists.TrackID = Tracks.TrackID ";
            joins += "LEFT JOIN Artists ON Artists.ArtistID = TrackArtists.ArtistID ";
            break;
        case(Filters::FilterType::Album):
            joins += "LEFT JOIN Albums ON Albums.AlbumID = Tracks.AlbumID ";
            break;
        case(Filters::FilterType::Genre):
            joins += "INNER JOIN TrackGenres ON TrackGenres.TrackID = Tracks.TrackID ";
            joins += "LEFT JOIN Genres ON Genres.GenreID = TrackGenres.GenreID ";
            break;
        case(Filters::FilterType::Year):
            break;
    }
    return joins;
}
QString getSearchJoins()
{
    QString joins;
    joins += "LEFT JOIN Artists AS AlbumArtists ON AlbumArtists.ArtistID = Tracks.AlbumArtistID ";
    joins += "LEFT JOIN Albums ON Albums.AlbumID = Tracks.AlbumID ";
    joins += "LEFT JOIN TrackArtists ON TrackArtists.TrackID = Tracks.TrackID ";
    joins += "LEFT JOIN Artists ON Artists.ArtistID = TrackArtists.ArtistID ";
    joins += "LEFT JOIN TrackGenres ON TrackGenres.TrackID = Tracks.TrackID ";
    joins += "LEFT JOIN Genres ON Genres.GenreID = TrackGenres.GenreID ";

    return joins;
}
} // namespace

namespace DB {
FilterDatabase::FilterDatabase(const QString& connectionName)
    : DB::Module(connectionName)
{ }

FilterDatabase::~FilterDatabase() = default;

bool FilterDatabase::getAllItems(Filters::FilterType type, ::Library::SortOrder order, FilterList& result) const
{
    const auto join = getFilterJoins(type);
    const auto queryText = fetchQueryItems(type, {}, join, order);
    if(!queryText.isEmpty()) {
        auto query = Query(module());
        query.prepareQuery(queryText);

        return dbFetchItems(query, result);
    }
    return false;
}

bool FilterDatabase::getItemsByFilter(Filters::FilterType type, const ActiveFilters& filters, const QString& search,
                                      ::Library::SortOrder order, FilterList& result) const
{
    if(!filters.isEmpty() || !search.isEmpty()) {
        QString where;
        QString join;

        for(const auto& [filter, ids] : asRange(filters)) {
            QString values;

            for(const auto& id : ids) {
                if(!values.isEmpty()) {
                    values += ",";
                }
                values += QString::number(id);
            }

            switch(filter) {
                case(Filters::FilterType::AlbumArtist): {
                    if(type != Filters::FilterType::AlbumArtist) {
                        if(!where.isEmpty()) {
                            where += " AND ";
                        }
                        where += QString("AlbumArtistID IN (%1)").arg(values);
                        if(search.isEmpty()) {
                            join += getFilterJoins(filter);
                        }
                    }
                    break;
                }
                case(Filters::FilterType::Artist): {
                    if(type != Filters::FilterType::Artist) {
                        if(!where.isEmpty()) {
                            where += " AND ";
                        }
                        where += QString("TrackArtists.ArtistID IN (%1)").arg(values);
                        if(search.isEmpty()) {
                            join += getFilterJoins(filter);
                        }
                    }
                    break;
                }
                case(Filters::FilterType::Album): {
                    if(type != Filters::FilterType::Album) {
                        if(!where.isEmpty()) {
                            where += " AND ";
                        }
                        where += QString("Tracks.AlbumID IN (%1)").arg(values);
                        if(search.isEmpty()) {
                            join += getFilterJoins(filter);
                        }
                    }
                    break;
                }
                case(Filters::FilterType::Year): {
                    if(type != Filters::FilterType::Year) {
                        if(!where.isEmpty()) {
                            where += " AND ";
                        }
                        where += QString("Tracks.Year IN (%1)").arg(values);
                    }
                    break;
                }
                case(Filters::FilterType::Genre): {
                    if(type != Filters::FilterType::Genre) {
                        if(!where.isEmpty()) {
                            where += " AND ";
                        }
                        where += QString("TrackGenres.GenreID IN (%1)").arg(values);
                        if(search.isEmpty()) {
                            join += getFilterJoins(filter);
                        }
                    }
                    break;
                }
            }
        }

        if(!search.isEmpty()) {
            if(!where.isEmpty()) {
                where += " AND ";
            }

            where += QString("(Tracks.Title LIKE '%%1%' OR Artists.Name LIKE '%1%%' "
                             "OR "
                             "AlbumArtists.Name LIKE '%%1%' OR Albums.Title LIKE "
                             "'%%1%')")
                         .arg(search.toLower());

            join += getSearchJoins();
        }
        else {
            join += getFilterJoins(type);
        }

        const auto queryText = fetchQueryItems(type, where, join, order);
        if(!queryText.isEmpty()) {
            auto query = Query(module());
            query.prepareQuery(queryText);

            return dbFetchItems(query, result);
        }
    }
    return false;
}

QString FilterDatabase::fetchQueryItems(Filters::FilterType type, const QString& where, const QString& join,
                                        Library::SortOrder order)
{
    QStringList fields{};
    QString group{};
    QString sortOrder{};

    switch(type) {
        case(Filters::FilterType::AlbumArtist):
            fields.append(QStringLiteral("AlbumArtists.ArtistID"));
            fields.append(QStringLiteral("AlbumArtists.Name"));
            group = QStringLiteral("AlbumArtists.ArtistID");
            switch(order) {
                case(Library::SortOrder::TitleDesc):
                    sortOrder = QStringLiteral("LOWER(AlbumArtists.Name) DESC");
                    break;
                case(Library::SortOrder::TitleAsc):
                    sortOrder = QStringLiteral("LOWER(AlbumArtists.Name)");
                    break;
                case(Library::SortOrder::YearDesc):
                    sortOrder = QStringLiteral("Tracks.Year, LOWER(AlbumArtists.Name)");
                    break;
                case(Library::SortOrder::YearAsc):
                    sortOrder = QStringLiteral("Tracks.Year ASC, LOWER(AlbumArtists.Name)");
                    break;
                case(Library::SortOrder::NoSorting):
                    break;
            }
            break;
        case(Filters::FilterType::Artist):
            fields.append(QStringLiteral("Artists.ArtistID"));
            fields.append(QStringLiteral("Artists.Name"));
            group = QStringLiteral("Artists.ArtistID");
            sortOrder = QStringLiteral("LOWER(Artists.Name)");
            break;
        case(Filters::FilterType::Album):
            fields.append(QStringLiteral("Albums.AlbumID"));
            fields.append(QStringLiteral("Albums.Title"));
            group = QStringLiteral("Albums.AlbumID");
            switch(order) {
                case(Library::SortOrder::TitleDesc):
                    sortOrder = QStringLiteral("LOWER(Albums.Title) DESC");
                    break;
                case(Library::SortOrder::TitleAsc):
                    sortOrder = QStringLiteral("LOWER(Albums.Title)");
                    break;
                case(Library::SortOrder::YearDesc):
                    sortOrder = QStringLiteral("Albums.Year DESC, LOWER(Albums.Title)");
                    break;
                case(Library::SortOrder::YearAsc):
                    sortOrder = QStringLiteral("Albums.Year ASC, LOWER(Albums.Title)");
                    break;
                case(Library::SortOrder::NoSorting):
                    sortOrder = QStringLiteral("LOWER(Albums.Title)");
                    break;
            }
            break;
        case(Filters::FilterType::Year):
            fields.append(QStringLiteral("Tracks.Year"));
            fields.append(QStringLiteral("Tracks.Year"));
            group = QStringLiteral("Tracks.Year");
            break;
        case(Filters::FilterType::Genre):
            fields.append(QStringLiteral("Genres.GenreID"));
            fields.append(QStringLiteral("Genres.Name"));
            group = QStringLiteral("Genres.GenreID");
            break;
    }

    const auto joinedFields = fields.join(", ");

    auto queryText = QString("SELECT %1 FROM Tracks %2 WHERE %3 GROUP BY %4 ORDER BY %5")
                         .arg(joinedFields, join.isEmpty() ? "" : join, where.isEmpty() ? "1" : where,
                              group.isEmpty() ? "1" : group, sortOrder.isEmpty() ? "1" : sortOrder);

    return queryText;
}

bool FilterDatabase::dbFetchItems(Query& q, FilterList& result)
{
    result.clear();

    if(!q.execQuery()) {
        q.error("Could not get all items from database");
        return false;
    }

    while(q.next()) {
        FilterEntry item;

        item.id = q.value(0).toInt();
        item.name = q.value(1).toString();

        result.append(item);
    }

    return true;
}

const DB::Module* FilterDatabase::module() const
{
    return this;
}
} // namespace DB
