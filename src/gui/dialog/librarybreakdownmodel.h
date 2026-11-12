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

#include <core/library/librarystatistics.h>

#include <QAbstractTableModel>

namespace Fooyin {
enum class LibraryBreakdown : uint8_t
{
    Codec = 0,
    BitDepth,
    SampleRate,
    Folder,
    Rating,
    Custom,
};

class LibraryBreakdownModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Role
    {
        SortRole = Qt::UserRole,
    };

    explicit LibraryBreakdownModel(QObject* parent = nullptr);
    void setGroups(std::vector<LibraryGroupStatistics> groups, LibraryBreakdown breakdown);

    [[nodiscard]] int rowCount(const QModelIndex& parent) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    [[nodiscard]] QString groupName(const LibraryGroupStatistics& group) const;

    std::vector<LibraryGroupStatistics> m_groups;
    LibraryBreakdown m_breakdown;
};
} // namespace Fooyin
