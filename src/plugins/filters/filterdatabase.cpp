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

#include "filterdatabase.h"

#include <core/database/query.h>

#include <QBuffer>
#include <QPixmap>

namespace Fy::Filters {
FilterDatabase::FilterDatabase(const QString& connectionName)
    : Core::DB::Module(connectionName)
{ }

bool FilterDatabase::getAllItems(Filters::FilterType type, Core::Library::SortOrder order, FilterEntries& result) const
{
    const auto queryText = fetchQueryItems(type, {}, {}, order);
    if(!queryText.isEmpty()) {
        auto query = Core::DB::Query(module());
        query.prepareQuery(queryText);

        return dbFetchItems(query, result);
    }
    return false;
}

bool FilterDatabase::getItemsByFilter(Filters::FilterType type, const ActiveFilters& filters, const QString& search,
                                      Core::Library::SortOrder order, FilterEntries& result) const
{
    if(!filters.empty() || !search.isEmpty()) {
        QString where;

        for(const auto& [filter, ids] : filters) {
            QString values;

            for(const auto& id : ids) {
                if(!values.isEmpty()) {
                    values += ",";
                }
                values += "\"" + id.name + "\"";
            }

            switch(filter) {
                case(Filters::FilterType::AlbumArtist): {
                    if(type != Filters::FilterType::AlbumArtist) {
                        if(!where.isEmpty()) {
                            where += " AND ";
                        }
                        where += QString("AlbumArtist IN (%1)").arg(values);
                    }
                    break;
                }
                case(Filters::FilterType::Artist): {
                    if(type != Filters::FilterType::Artist) {
                        if(!where.isEmpty()) {
                            where += " AND ";
                        }
                        where += QString("Artists IN (%1)").arg(values);
                    }
                    break;
                }
                case(Filters::FilterType::Album): {
                    if(type != Filters::FilterType::Album) {
                        if(!where.isEmpty()) {
                            where += " AND ";
                        }
                        where += QString("Album IN (%1)").arg(values);
                    }
                    break;
                }
                case(Filters::FilterType::Year): {
                    if(type != Filters::FilterType::Year) {
                        if(!where.isEmpty()) {
                            where += " AND ";
                        }
                        where += QString("SUBSTRING(Date, 1, 4) IN (%1)").arg(values);
                    }
                    break;
                }
                case(Filters::FilterType::Genre): {
                    if(type != Filters::FilterType::Genre) {
                        if(!where.isEmpty()) {
                            where += " AND ";
                        }
                        where += QString("Genres IN (%1)").arg(values);
                    }
                    break;
                }
            }
        }

        if(!search.isEmpty()) {
            if(!where.isEmpty()) {
                where += " AND ";
            }

            where += QString("(Title LIKE '%%1%' OR Artists LIKE '%1%%' "
                             "OR "
                             "AlbumArtist LIKE '%%1%' OR Album LIKE "
                             "'%%1%')")
                         .arg(search.toLower());
        }

        const auto queryText = fetchQueryItems(type, where, {}, order);
        if(!queryText.isEmpty()) {
            auto query = Core::DB::Query(module());
            query.prepareQuery(queryText);

            return dbFetchItems(query, result);
        }
    }
    return false;
}

QString FilterDatabase::fetchQueryItems(Filters::FilterType type, const QString& where, const QString& join,
                                        Core::Library::SortOrder order)
{
    QStringList fields{};
    QString group{};
    QString sortOrder{};

    switch(type) {
        case(Filters::FilterType::AlbumArtist):
            fields.append(QStringLiteral("AlbumArtist"));
            group = QStringLiteral("AlbumArtist");
            switch(order) {
                    //                case(Core::Library::SortOrder::TitleDesc):
                    //                    sortOrder = QStringLiteral("LOWER(AlbumArtists.Name) DESC");
                    //                    break;
                    //                case(Core::Library::SortOrder::TitleAsc):
                    //                    sortOrder = QStringLiteral("LOWER(AlbumArtists.Name)");
                    //                    break;
                    //                case(Core::Library::SortOrder::YearDesc):
                    //                    sortOrder = QStringLiteral("Tracks.Date, LOWER(AlbumArtists.Name)");
                    //                    break;
                    //                case(Core::Library::SortOrder::YearAsc):
                    //                    sortOrder = QStringLiteral("Tracks.Date ASC, LOWER(AlbumArtists.Name)");
                    //                    break;
                case(Core::Library::SortOrder::NoSorting):
                    sortOrder = QStringLiteral("LOWER(AlbumArtist)");
                    break;
            }
            break;
        case(Filters::FilterType::Artist):
            fields.append(QStringLiteral("Artists"));
            group     = QStringLiteral("Artists");
            sortOrder = QStringLiteral("LOWER(Artists)");
            break;
        case(Filters::FilterType::Album):
            fields.append(QStringLiteral("Album"));
            group = QStringLiteral("Album");
            switch(order) {
                    //                case(Core::Library::SortOrder::TitleDesc):
                    //                    sortOrder = QStringLiteral("LOWER(Albums.Title) DESC");
                    //                    break;
                    //                case(Core::Library::SortOrder::TitleAsc):
                    //                    sortOrder = QStringLiteral("LOWER(Albums.Title)");
                    //                    break;
                    //                case(Core::Library::SortOrder::YearDesc):
                    //                    sortOrder = QStringLiteral("Albums.Date DESC, LOWER(Albums.Title)");
                    //                    break;
                    //                case(Core::Library::SortOrder::YearAsc):
                    //                    sortOrder = QStringLiteral("Albums.Date ASC, LOWER(Albums.Title)");
                    //                    break;
                case(Core::Library::SortOrder::NoSorting):
                    sortOrder = QStringLiteral("LOWER(Album)");
                    break;
            }
            break;
        case(Filters::FilterType::Year):
            fields.append(QStringLiteral("SUBSTRING(Tracks.Date, 1, 4)"));
            group = QStringLiteral("SUBSTRING(Tracks.Date, 1, 4)");
            break;
        case(Filters::FilterType::Genre):
            fields.append(QStringLiteral("Genres"));
            group = QStringLiteral("Genres");
            break;
    }

    const auto joinedFields = fields.join(", ");

    auto queryText = QString("SELECT %1 FROM Tracks %2 WHERE %3 GROUP BY %4 ORDER BY %5")
                         .arg(joinedFields, join.isEmpty() ? "" : join, where.isEmpty() ? "1" : where,
                              group.isEmpty() ? "1" : group, sortOrder.isEmpty() ? "1" : sortOrder);

    return queryText;
}

bool FilterDatabase::dbFetchItems(Core::DB::Query& q, FilterEntries& result)
{
    result.clear();

    if(!q.execQuery()) {
        q.error("Could not get all items from database");
        return false;
    }

    while(q.next()) {
        FilterEntry item;

        item.id   = result.size();
        item.name = q.value(0).toString();

        result.emplace_back(item);
    }

    return true;
}

const Core::DB::Module* FilterDatabase::module() const
{
    return this;
}
} // namespace Fy::Filters
