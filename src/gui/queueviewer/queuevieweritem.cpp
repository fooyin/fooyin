/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "queuevieweritem.h"

#include <core/scripting/scriptparser.h>

namespace Fooyin {
QueueViewerItem::QueueViewerItem(PlaylistTrack track)
    : m_track{std::move(track)}
{ }

QString QueueViewerItem::title() const
{
    return m_title;
}

QString QueueViewerItem::subtitle() const
{
    return m_subtitle;
}

PlaylistTrack QueueViewerItem::track() const
{
    return m_track;
}

void QueueViewerItem::generateTitle(ScriptParser* parser, const QString& leftScript, const QString& rightScript)
{
    if(!parser) {
        return;
    }

    m_title    = parser->evaluate(leftScript, m_track.track);
    m_subtitle = parser->evaluate(rightScript, m_track.track);
}
} // namespace Fooyin
