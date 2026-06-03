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

#include "scriptvariableproviders.h"

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

const Fooyin::ScriptPlaylistEnvironment* playlistEnvironment(const Fooyin::ScriptContext& context)
{
    return context.environment ? context.environment->playlistEnvironment() : nullptr;
}

QString trackDepth(const Fooyin::ScriptContext& context)
{
    const auto* environment = playlistEnvironment(context);
    if(environment == nullptr) {
        return {};
    }

    return QString::number(environment->trackDepth());
}

QString trackIndex(const Fooyin::ScriptContext& context)
{
    const auto* environment = playlistEnvironment(context);
    if(environment == nullptr) {
        return {};
    }

    const int playlistTrackIndex = environment->currentPlaylistTrackIndex();
    if(playlistTrackIndex < 0) {
        return {};
    }

    const int playlistTrackCount = environment->playlistTrackCount();
    const int numDigits          = playlistTrackCount > 0 ? static_cast<int>(std::log10(playlistTrackCount)) + 1 : 2;

    return Fooyin::Utils::addLeadingZero(playlistTrackIndex + 1, numDigits);
}

QString queueIndex(const Fooyin::ScriptContext& context)
{
    const auto* environment = playlistEnvironment(context);
    if(environment == nullptr) {
        return {};
    }

    const auto indexes = environment->currentQueueIndexes();
    if(indexes.empty()) {
        return {};
    }

    return QString::number(indexes.front() + 1);
}

QString queueIndexes(const Fooyin::ScriptContext& context)
{
    const auto* environment = playlistEnvironment(context);
    if(environment == nullptr) {
        return {};
    }

    const auto indexes = environment->currentQueueIndexes();
    if(indexes.empty()) {
        return {};
    }

    QStringList textIndexes;
    textIndexes.reserve(static_cast<qsizetype>(indexes.size()));

    for(const int index : indexes) {
        textIndexes.append(QString::number(index + 1));
    }

    return textIndexes.join(u", "_s);
}

QString queueTotal(const Fooyin::ScriptContext& context)
{
    const auto* environment = playlistEnvironment(context);
    if(environment == nullptr || environment->currentQueueIndexes().empty()) {
        return {};
    }

    return QString::number(environment->currentQueueTotal());
}

QString playingQueue(const Fooyin::ScriptContext& context)
{
    const QString indexes = queueIndexes(context);
    return indexes.isEmpty() ? QString{} : u"[%1]"_s.arg(indexes);
}
} // namespace

namespace Fooyin {
const ScriptVariableProvider& artworkMarkerVariableProvider()
{
    static const StaticScriptVariableProvider Provider{
        makeScriptVariableDescriptor<frontCover>(VariableKind::FrontCover, u"FRONTCOVER"_s),
        makeScriptVariableDescriptor<backCover>(VariableKind::BackCover, u"BACKCOVER"_s),
        makeScriptVariableDescriptor<artistPicture>(VariableKind::ArtistPicture, u"ARTISTPICTURE"_s)};
    return Provider;
}

const ScriptVariableProvider& playlistVariableProvider()
{
    static const StaticScriptVariableProvider Provider{
        makeScriptVariableDescriptor<trackDepth>(VariableKind::Depth, u"DEPTH"_s),
        makeScriptVariableDescriptor<trackIndex>(VariableKind::ListIndex, u"LIST_INDEX"_s),
        makeScriptVariableDescriptor<queueIndex>(VariableKind::QueueIndex, u"QUEUEINDEX"_s),
        makeScriptVariableDescriptor<queueIndex>(VariableKind::QueueIndex, u"QUEUE_INDEX"_s),
        makeScriptVariableDescriptor<queueIndexes>(VariableKind::QueueIndexes, u"QUEUEINDEXES"_s),
        makeScriptVariableDescriptor<queueIndexes>(VariableKind::QueueIndexes, u"QUEUE_INDEXES"_s),
        makeScriptVariableDescriptor<queueTotal>(VariableKind::QueueTotal, u"QUEUETOTAL"_s),
        makeScriptVariableDescriptor<queueTotal>(VariableKind::QueueTotal, u"QUEUE_TOTAL"_s),
        makeScriptVariableDescriptor<playingQueue>(VariableKind::PlayingIcon, u"PLAYINGICON"_s)};
    return Provider;
}
} // namespace Fooyin
