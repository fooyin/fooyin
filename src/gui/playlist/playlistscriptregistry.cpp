/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "playlistscriptregistry.h"

#include <core/constants.h>
#include <utils/stringutils.h>

using namespace Qt::StringLiterals;

namespace {
QString frontCover()
{
    return QString::fromLatin1(Fooyin::Constants::FrontCover);
}

QString backCover()
{
    return QString::fromLatin1(Fooyin::Constants::BackCover);
}

QString artistPicture()
{
    return QString::fromLatin1(Fooyin::Constants::ArtistPicture);
}
} // namespace

namespace Fooyin {
class PlaylistScriptRegistryPrivate
{
public:
    QString depth() const;

    QString trackIndex() const;
    QString queueIndex() const;
    QString queueIndexes() const;
    QString playingQueue() const;

    PlaylistTrackIndexes m_playlistQueue;
    Playlist* m_playlist;
    UId m_playlistId;
    int m_trackIndex{0};
    int m_trackDepth{0};
    mutable int m_trackCount{0};
    mutable int m_numDigits{2};

    using PlaylistVar  = std::function<QString()>;
    using PlaylistVars = std::unordered_map<QString, PlaylistVar>;
    // clang-format off
    PlaylistVars m_vars{{u"depth"_s, [this]() { return depth(); }},
                        {u"list_index"_s, [this]() { return trackIndex(); }},
                        {u"queueindex"_s, [this]() { return queueIndex(); }},
                        {u"queueindexes"_s, [this]() { return queueIndexes(); }},
                        {u"playingicon"_s, [this]() { return playingQueue(); }},
                        {u"frontcover"_s, frontCover},
                        {u"backcover"_s, backCover},
                        {u"artistpicture"_s, artistPicture}};
    // clang-format on
};

QString PlaylistScriptRegistryPrivate::depth() const
{
    return QString::number(m_trackDepth);
}

QString PlaylistScriptRegistryPrivate::trackIndex() const
{
    if(m_playlist && std::exchange(m_trackCount, m_playlist->trackCount()) != m_trackCount) {
        m_numDigits = m_trackCount > 0 ? static_cast<int>(std::log10(m_trackCount)) + 1 : 2;
    }

    return Utils::addLeadingZero(m_trackIndex + 1, m_numDigits);
}

QString PlaylistScriptRegistryPrivate::queueIndex() const
{
    if(!m_playlistQueue.contains(m_trackIndex)) {
        return {};
    }

    return QString::number(*m_playlistQueue.at(m_trackIndex).begin());
}

QString PlaylistScriptRegistryPrivate::queueIndexes() const
{
    if(!m_playlistQueue.contains(m_trackIndex)) {
        return {};
    }

    const auto& indexes = m_playlistQueue.at(m_trackIndex);

    QStringList str;
    str.reserve(static_cast<qsizetype>(indexes.size()));

    for(const int index : indexes) {
        str.append(QString::number(index + 1));
    }

    return str.join(u", "_s);
}

QString PlaylistScriptRegistryPrivate::playingQueue() const
{
    const QString indexes = queueIndexes();
    return indexes.isEmpty() ? QString{} : u"[%1]"_s.arg(indexes);
}

PlaylistScriptRegistry::PlaylistScriptRegistry()
    : p{std::make_unique<PlaylistScriptRegistryPrivate>()}
{ }

PlaylistScriptRegistry::~PlaylistScriptRegistry() = default;

void PlaylistScriptRegistry::setup(Playlist* playlist, const PlaybackQueue& queue)
{
    p->m_playlist = playlist;
    if(playlist) {
        p->m_playlistId = playlist->id();
        if(p->m_playlistId.isValid()) {
            p->m_playlistQueue = queue.indexesForPlaylist(p->m_playlistId);
        }
    }
}

void PlaylistScriptRegistry::setTrackProperties(int index, int depth)
{
    p->m_trackIndex = index;
    p->m_trackDepth = depth;
}

bool PlaylistScriptRegistry::isVariable(const QString& var, const Track& track) const
{
    if(isListVariable(var) || p->m_vars.contains(var)) {
        return true;
    }

    return ScriptRegistry::isVariable(var, track);
}

ScriptResult PlaylistScriptRegistry::value(const QString& var, const Track& track) const
{
    if(isListVariable(var)) {
        return {.value = u"|Loading|"_s, .cond = true};
    }

    if(p->m_vars.contains(var)) {
        ScriptResult result;
        result.value = p->m_vars.at(var)();
        result.cond  = !result.value.isEmpty();
        return result;
    }

    return ScriptRegistry::value(var, track);
}

ScriptResult PlaylistScriptRegistry::calculateResult(FuncRet funcRet) const
{
    ScriptResult result = ScriptRegistry::calculateResult(funcRet);
    result.value.replace(u'<', u"\\<"_s);
    return result;
}
} // namespace Fooyin
