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

namespace Fooyin {
struct PlaylistScriptRegistry::Private
{
    PlaylistIndexes playlistQueue;

    Id playlistId;
    int trackIndex{0};

    using QueueVar = std::function<QString()>;
    std::unordered_map<QString, QueueVar> vars;

    Private()
    {
        addVars();
    }

    void addVars()
    {
        vars.emplace(QStringLiteral("queueindex"), [this]() { return queueIndex(); });
        vars.emplace(QStringLiteral("queueindexes"), [this]() { return queueIndexes(); });
        vars.emplace(QStringLiteral("playingicon"), [this]() { return playingQueue(); });
        vars.emplace(QStringLiteral("frontcover"), [this]() { return frontCover(); });
        vars.emplace(QStringLiteral("backcover"), [this]() { return backCover(); });
        vars.emplace(QStringLiteral("artistpicture"), [this]() { return artistPicture(); });
    }

    QString frontCover()
    {
        return QString::fromLatin1(FrontCover);
    }

    QString backCover()
    {
        return QString::fromLatin1(BackCover);
    }

    QString artistPicture()
    {
        return QString::fromLatin1(ArtistPicture);
    }

    QString playingQueue()
    {
        const QString indexes = queueIndexes();
        return indexes.isEmpty() ? QString{} : QStringLiteral("[%1]").arg(indexes);
    }

    QString queueIndex()
    {
        if(!playlistQueue.contains(trackIndex)) {
            return {};
        }

        return QString::number(*playlistQueue.at(trackIndex).begin());
    }

    QString queueIndexes()
    {
        if(!playlistQueue.contains(trackIndex)) {
            return {};
        }

        const auto& indexes = playlistQueue.at(trackIndex);

        QStringList str;
        str.reserve(indexes.size());

        for(const int index : indexes) {
            str.append(QString::number(index + 1));
        }

        return str.join(QStringLiteral(", "));
    }
};

PlaylistScriptRegistry::PlaylistScriptRegistry()
    : p{std::make_unique<Private>()}
{ }

PlaylistScriptRegistry::~PlaylistScriptRegistry() = default;

void PlaylistScriptRegistry::setup(const Id& playlistId, const PlaybackQueue& queue)
{
    p->playlistId = playlistId;
    if(playlistId.isValid()) {
        p->playlistQueue = queue.indexesForPlaylist(playlistId);
    }
}

void PlaylistScriptRegistry::setTrackIndex(int index)
{
    p->trackIndex = index;
}

bool PlaylistScriptRegistry::isVariable(const QString& var, const Track& track) const
{
    if(isListVariable(var) || p->vars.contains(var)) {
        return true;
    }

    return ScriptRegistry::isVariable(var, track);
}

ScriptResult PlaylistScriptRegistry::value(const QString& var, const Track& track) const
{
    if(isListVariable(var)) {
        return {.value = QStringLiteral("|Loading|"), .cond = true};
    }

    if(p->vars.contains(var)) {
        ScriptResult result;
        result.value = p->vars.at(var)();
        result.cond  = !result.value.isEmpty();
        return result;
    }

    return ScriptRegistry::value(var, track);
}
} // namespace Fooyin
