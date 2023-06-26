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

#include "librarytreegroup.h"

#include <QObject>

namespace Fy {
namespace Utils {
class SettingsManager;
}

namespace Gui::Widgets {
using IndexGroupMap = std::map<int, LibraryTreeGrouping>;

class LibraryTreeGroupRegistry : public QObject
{
    Q_OBJECT

public:
    explicit LibraryTreeGroupRegistry(Utils::SettingsManager* settings, QObject* parent = nullptr);

    [[nodiscard]] const IndexGroupMap& groupings() const;

    LibraryTreeGrouping addGrouping(const LibraryTreeGrouping& grouping);
    bool changeGrouping(const LibraryTreeGrouping& grouping);

    [[nodiscard]] LibraryTreeGrouping groupingByIndex(int index) const;
    [[nodiscard]] LibraryTreeGrouping groupingByName(const QString& name) const;

    bool removeByIndex(int index);

    void saveGroupings();
    void loadGroupings();

signals:
    void groupingChanged(const LibraryTreeGrouping& oldGroup, const LibraryTreeGrouping& newGroup);

private:
    Utils::SettingsManager* m_settings;

    IndexGroupMap m_groupings;
};

QDataStream& operator<<(QDataStream& stream, const LibraryTreeGrouping& group);
QDataStream& operator>>(QDataStream& stream, LibraryTreeGrouping& group);
QDataStream& operator<<(QDataStream& stream, const IndexGroupMap& groupMap);
QDataStream& operator>>(QDataStream& stream, IndexGroupMap& groupMap);
} // namespace Gui::Widgets
} // namespace Fy
