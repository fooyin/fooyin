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
    int trackDepth{0};

    using QueueVar = std::function<QString()>;
    std::unordered_map<QString, QueueVar> vars;

    Private()
    {
        addVars();
    }

    void addVars()
    {
        vars.emplace(QStringLiteral("depth"), [this]() { return depth(); });
        vars.emplace(QStringLiteral("queueindex"), [this]() { return queueIndex(); });
        vars.emplace(QStringLiteral("queueindexes"), [this]() { return queueIndexes(); });
        vars.emplace(QStringLiteral("playingicon"), [this]() { return playingQueue(); });
        vars.emplace(QStringLiteral("frontcover"), []() { return frontCover(); });
        vars.emplace(QStringLiteral("backcover"), []() { return backCover(); });
        vars.emplace(QStringLiteral("artistpicture"), []() { return artistPicture(); });
    }

    QString depth() const
    {
        return QString::number(trackDepth);
    }

    QString queueIndex() const
    {
        if(!playlistQueue.contains(trackIndex)) {
            return {};
        }

        return QString::number(*playlistQueue.at(trackIndex).begin());
    }

    QString queueIndexes() const
    {
        if(!playlistQueue.contains(trackIndex)) {
            return {};
        }

        const auto& indexes = playlistQueue.at(trackIndex);

        QStringList str;
        str.reserve(static_cast<qsizetype>(indexes.size()));

        for(const int index : indexes) {
            str.append(QString::number(index + 1));
        }

        return str.join(QStringLiteral(", "));
    }

    QString playingQueue() const
    {
        const QString indexes = queueIndexes();
        return indexes.isEmpty() ? QString{} : QStringLiteral("[%1]").arg(indexes);
    }

    static QString frontCover()
    {
        return QString::fromLatin1(FrontCover);
    }

    static QString backCover()
    {
        return QString::fromLatin1(BackCover);
    }

    static QString artistPicture()
    {
        return QString::fromLatin1(ArtistPicture);
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

void PlaylistScriptRegistry::setTrackProperties(int index, int depth)
{
    p->trackIndex = index;
    p->trackDepth = depth;
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
