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
    PlaylistVars m_vars{{u"DEPTH"_s, [this]() { return depth(); }},
                        {u"LIST_INDEX"_s, [this]() { return trackIndex(); }},
                        {u"QUEUEINDEX"_s, [this]() { return queueIndex(); }},
                        {u"QUEUEINDEXES"_s, [this]() { return queueIndexes(); }},
                        {u"PLAYINGICON"_s, [this]() { return playingQueue(); }},
                        {u"FRONTCOVER"_s, frontCover},
                        {u"BACKCOVER"_s, backCover},
                        {u"ARTISTPICTURE"_s, artistPicture}};
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

VariableKind PlaylistScriptRegistry::resolveVariableKind(const QString& var) const
{
    if(var == "DEPTH"_L1) {
        return VariableKind::Depth;
    }
    if(var == "LIST_INDEX"_L1) {
        return VariableKind::ListIndex;
    }
    if(var == "QUEUEINDEX"_L1) {
        return VariableKind::QueueIndex;
    }
    if(var == "QUEUEINDEXES"_L1) {
        return VariableKind::QueueIndexes;
    }
    if(var == "PLAYINGICON"_L1) {
        return VariableKind::PlayingIcon;
    }
    if(var == "FRONTCOVER"_L1) {
        return VariableKind::FrontCover;
    }
    if(var == "BACKCOVER"_L1) {
        return VariableKind::BackCover;
    }
    if(var == "ARTISTPICTURE"_L1) {
        return VariableKind::ArtistPicture;
    }

    return ScriptRegistry::resolveVariableKind(var);
}

bool PlaylistScriptRegistry::isVariable(const QString& var, const Track& track) const
{
    if(resolveVariableKind(var) != VariableKind::Generic || isListVariable(var)) {
        return true;
    }

    return ScriptRegistry::isVariable(var, track);
}

ScriptResult PlaylistScriptRegistry::value(VariableKind kind, const QString& var, const Track& track) const
{
    switch(kind) {
        case VariableKind::Depth:
            return {.value = p->depth(), .cond = true};
        case VariableKind::ListIndex: {
            const QString value = p->trackIndex();
            return {.value = value, .cond = !value.isEmpty()};
        }
        case VariableKind::QueueIndex: {
            const QString value = p->queueIndex();
            return {.value = value, .cond = !value.isEmpty()};
        }
        case VariableKind::QueueIndexes: {
            const QString value = p->queueIndexes();
            return {.value = value, .cond = !value.isEmpty()};
        }
        case VariableKind::PlayingIcon: {
            const QString value = p->playingQueue();
            return {.value = value, .cond = !value.isEmpty()};
        }
        case VariableKind::FrontCover:
            return {.value = frontCover(), .cond = true};
        case VariableKind::BackCover:
            return {.value = backCover(), .cond = true};
        case VariableKind::ArtistPicture:
            return {.value = artistPicture(), .cond = true};
        case VariableKind::TrackCount:
        case VariableKind::Playtime:
        case VariableKind::PlaylistDuration:
            return {.value = u"|Loading|"_s, .cond = true};
        case VariableKind::Generic:
        case VariableKind::Track:
        case VariableKind::Disc:
        case VariableKind::DiscTotal:
        case VariableKind::Title:
        case VariableKind::UniqueArtist:
        case VariableKind::PlayCount:
        case VariableKind::Duration:
        case VariableKind::AlbumArtist:
        case VariableKind::Album:
        case VariableKind::Genres:
            break;
    }

    return ScriptRegistry::value(kind, var, track);
}

ScriptResult PlaylistScriptRegistry::value(const QString& var, const Track& track) const
{
    return value(resolveVariableKind(var), var, track);
}

ScriptResult PlaylistScriptRegistry::calculateResult(const FuncRet& funcRet) const
{
    ScriptResult result = ScriptRegistry::calculateResult(funcRet);
    result.value.replace(u'<', u"\\<"_s);
    return result;
}
} // namespace Fooyin
