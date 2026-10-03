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

#include "queuevieweritem.h"

#include <core/scripting/scriptparser.h>
#include <gui/scripting/richtextutils.h>
#include <gui/scripting/scriptformatter.h>

namespace Fooyin {
QueueViewerItem::QueueViewerItem(PlaylistTrack track, PlaybackQueueItemId queueItemId)
    : m_track{std::move(track)}
    , m_queueItemId{queueItemId}
{ }

QString QueueViewerItem::title() const
{
    return m_title;
}

const RichText& QueueViewerItem::richTitle() const
{
    return m_richTitle;
}

const RichText& QueueViewerItem::rightRichTitle() const
{
    return m_rightRichTitle;
}

PlaylistTrack QueueViewerItem::track() const
{
    return m_track;
}

PlaybackQueueItemId QueueViewerItem::queueItemId() const
{
    return m_queueItemId;
}

void QueueViewerItem::generateTitle(ScriptParser* parser, ScriptFormatter* formatter, const QString& displayScript,
                                    const ScriptContext& context)
{
    if(!parser || !formatter) {
        return;
    }

    const auto text  = formatter->evaluate(parser->evaluate(displayScript, m_track.track, context));
    m_title          = text.joinedText();
    m_richTitle      = trimRichText(richTextForAlignment(text, RichAlignment::Left));
    m_rightRichTitle = trimRichText(richTextForAlignment(text, RichAlignment::Right));
}
} // namespace Fooyin
