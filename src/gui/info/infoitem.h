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

#include <utils/treeitem.h>

#include <QString>

namespace Fy::Gui::Widgets {
class InfoItem : public Utils::TreeItem<InfoItem>
{
public:
    enum Type
    {
        Header = 0,
        Entry  = 1,
    };

    enum Role
    {
        None        = 0,
        Title       = 1,
        Artist      = 2,
        Album       = 3,
        Year        = 4,
        Genre       = 5,
        TrackNumber = 6,
        Filename    = 7,
        Path        = 8,
        Duration    = 9,
        Bitrate     = 10,
        SampleRate  = 11,
    };

    explicit InfoItem(Type type = Header, Role role = Title, QString title = {}, InfoItem* parent = nullptr);

    [[nodiscard]] int columnCount() const override;
    [[nodiscard]] QString data() const;
    [[nodiscard]] Type type() const;
    [[nodiscard]] Role role() const;

private:
    Type m_type;
    Role m_role;
    QString m_title;
};
} // namespace Fy::Gui::Widgets
