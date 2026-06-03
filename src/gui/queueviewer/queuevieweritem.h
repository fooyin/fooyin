/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include <core/playlist/playlist.h>
#include <core/scripting/scripttypes.h>
#include <gui/scripting/richtext.h>
#include <utils/treeitem.h>

namespace Fooyin {
class ScriptParser;
class ScriptFormatter;

class QueueViewerItem : public TreeItem<QueueViewerItem>
{
public:
    enum Role
    {
        RightText = Qt::UserRole,
        RichTitle,
        RichRightText,
        Track
    };

    QueueViewerItem() = default;
    explicit QueueViewerItem(PlaylistTrack track);

    [[nodiscard]] QString title() const;
    [[nodiscard]] QString subtitle() const;
    [[nodiscard]] const RichText& richTitle() const;
    [[nodiscard]] const RichText& richSubtitle() const;
    [[nodiscard]] PlaylistTrack track() const;

    void generateTitle(ScriptParser* parser, ScriptFormatter* formatter, const QString& leftScript,
                       const QString& rightScript, const ScriptContext& context);

private:
    RichText m_title;
    RichText m_subtitle;
    PlaylistTrack m_track;
};
} // namespace Fooyin
