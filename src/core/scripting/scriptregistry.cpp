/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include <core/scripting/scriptregistry.h>

#include "functions/controlfuncs.h"
#include "functions/mathfuncs.h"
#include "functions/stringfuncs.h"
#include "functions/timefuncs.h"
#include "functions/tracklistfuncs.h"
#include "scriptevaluator.h"
#include "version.h"

#include <core/constants.h>
#include <unordered_map>
#include <unordered_set>
#include <utils/stringutils.h>
#include <utils/utils.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
QString trackMeta(const ScriptFunctionCallContext& call)
{
    if(call.args.empty() || !call.subject.track) {
        return {};
    }

    const QString tag = call.args.front().value.toUpper();
    if(tag == QLatin1StringView{Constants::MetaData::RatingStars}) {
        return escapeRichText(
            call.options,
            formattedRatingStars(*call.subject.track, call.options ? *call.options : ScriptEvaluationOptions{}, false));
    }
    if(tag == QLatin1StringView{Constants::MetaData::RatingStarsPadded}) {
        return escapeRichText(
            call.options,
            formattedRatingStars(*call.subject.track, call.options ? *call.options : ScriptEvaluationOptions{}, true));
    }

    QStringList values = call.subject.track->metaValues(tag);
    for(auto& value : values) {
        value = escapeRichText(call.options, value);
    }
    if(call.args.size() == 1) {
        return values.join(", "_L1);
    }

    bool indexOk{false};
    const int index = call.args[1].value.toInt(&indexOk);
    if(!indexOk || index < 0 || std::cmp_greater_equal(index, values.size())) {
        return {};
    }
    return values.at(index);
}

QString trackMetaSep(const ScriptFunctionCallContext& call)
{
    if(call.args.size() < 2 || call.args.size() > 3 || !call.subject.track) {
        return {};
    }

    QStringList values = call.subject.track->metaValues(call.args.front().value);
    for(auto& value : values) {
        value = escapeRichText(call.options, value);
    }
    if(values.size() < 2) {
        return values.value(0);
    }

    const QString sep = call.args[1].value;
    if(call.args.size() == 2) {
        return values.join(sep);
    }

    QString result;
    for(qsizetype i{0}; i < values.size(); ++i) {
        if(i > 0) {
            result.append(i + 1 == values.size() ? call.args[2].value : sep);
        }
        result.append(values.at(i));
    }
    return result;
}

ScriptResult trackMetaTest(const ScriptFunctionCallContext& call)
{
    if(call.args.empty() || !call.subject.track) {
        return {};
    }

    for(const auto& arg : call.args) {
        if(call.subject.track->metaValues(arg.value).empty()) {
            return {};
        }
    }

    return {.value = u"1"_s, .cond = true};
}

QString trackMetaNum(const ScriptFunctionCallContext& call)
{
    if(call.args.size() != 1 || !call.subject.track) {
        return {};
    }

    return QString::number(call.subject.track->metaValues(call.args.front().value).size());
}

QString trackInfo(const ScriptFunctionCallContext& call)
{
    if(call.args.empty() || !call.subject.track) {
        return {};
    }

    return escapeRichText(call.options, call.subject.track->techInfo(call.args.front().value));
}

ScriptResult dateTimeVariable(const ScriptContext& context)
{
    for(const auto& value : context.values) {
        if(value.name == u"DATETIME") {
            return value.result;
        }
    }

    const QString value = QDateTime::currentDateTime().toString(u"yyyy-MM-dd hh:mm:ss"_s);
    return {.value = value, .cond = !value.isEmpty()};
}

QString fooyinVersionVariable()
{
    return QString::fromLatin1(VERSION);
}

QString frontCover()
{
    return QString::fromLatin1(Constants::FrontCover);
}

QString backCover()
{
    return QString::fromLatin1(Constants::BackCover);
}

QString artistPicture()
{
    return QString::fromLatin1(Constants::ArtistPicture);
}

const ScriptPlaylistData* playlistData(const ScriptContext& context)
{
    return context.playlistData;
}

QString trackDepth(const ScriptContext& context)
{
    const auto* row = playlistData(context);
    if(row == nullptr) {
        return {};
    }

    return QString::number(row->depth);
}

QString trackIndex(const ScriptContext& context)
{
    const auto* row = playlistData(context);
    if(row == nullptr) {
        return {};
    }

    const int playlistTrackIndex = row->trackIndex;
    if(playlistTrackIndex < 0) {
        return {};
    }

    const int playlistTrackCount = row->trackCount;
    const int numDigits          = playlistTrackCount > 0 ? static_cast<int>(std::log10(playlistTrackCount)) + 1 : 2;

    return Utils::addLeadingZero(playlistTrackIndex + 1, numDigits);
}

QString queueIndex(const ScriptContext& context)
{
    const auto* row = playlistData(context);
    if(row == nullptr) {
        return {};
    }

    const auto& indexes = row->queueIndexes;
    if(indexes.empty()) {
        return {};
    }

    return QString::number(indexes.front() + 1);
}

QString queueIndexes(const ScriptContext& context)
{
    const auto* row = playlistData(context);
    if(row == nullptr) {
        return {};
    }

    const auto& indexes = row->queueIndexes;
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

QString queueTotal(const ScriptContext& context)
{
    const auto* row = playlistData(context);
    if(row == nullptr || row->queueIndexes.empty()) {
        return {};
    }

    return QString::number(row->queueTotal);
}

QString queueMode(const ScriptContext& context)
{
    if(!context.playback) {
        return {};
    }

    switch(context.playback->queueMode) {
        case PlaybackQueueMode::PlaylistWithOverrides:
            return u"playlist"_s;
        case PlaybackQueueMode::QueueAsPlaybackSource:
            return u"queue"_s;
    }
    return {};
}

QString playingIcon(const ScriptContext& context)
{
    const auto* row = playlistData(context);
    return row && row->playingIcon != ScriptPlayingIcon::None ? QString{Constants::PlayingIconMarker} : QString{};
}

template <ScriptQueueItemState State>
QString queueItemState(const ScriptContext& context)
{
    const auto* row = playlistData(context);
    return row && row->queueItemState == State ? u"1"_s : QString{};
}

ScriptDependencies builtInVariableDependencies(VariableKind kind, const QString& name)
{
    using namespace ScriptSources;

    if(kind >= VariableKind::InputDecoder && kind <= VariableKind::OutputBufferLength) {
        return {.sources = {PlaybackOutput, PlaybackTrack}, .unclassified = false};
    }

    switch(kind) {
        case VariableKind::Generic:
            return name == u"DATETIME" ? ScriptDependencies{.sources = {Clock}, .unclassified = false}
                                       : ScriptDependencies{.sources = {}, .unclassified = false};
        case VariableKind::PlaybackTime:
        case VariableKind::PlaybackTimeSeconds:
        case VariableKind::PlaybackTimeRemaining:
        case VariableKind::PlaybackTimeRemainingSeconds:
            return {.sources = {PlaybackTime, PlaybackTrack, TrackMetadata}, .unclassified = false};
        case VariableKind::IsPlaying:
        case VariableKind::IsPaused:
        case VariableKind::PlayingIcon:
            return {.sources = {PlaybackTrack}, .unclassified = false};
        case VariableKind::IsStopped:
            return {.sources = {PlaybackStopped}, .unclassified = false};
        case VariableKind::QueueIndex:
        case VariableKind::QueueIndexes:
        case VariableKind::QueueTotal:
        case VariableKind::QueueMode:
            return {.sources = {Queue}, .unclassified = false};
        case VariableKind::IsQueueCurrent:
        case VariableKind::IsQueueUpcoming:
        case VariableKind::IsQueuePlayed:
            return {.sources = {Queue, QueueCursor}, .unclassified = false};
        case VariableKind::PlaylistElapsed:
            return {.sources = {ScriptSources::Playlist, TrackMetadata, PlaylistPlayback}, .unclassified = false};
        case VariableKind::TrackCount:
        case VariableKind::AverageBitrate:
        case VariableKind::Playtime:
        case VariableKind::Genres:
        case VariableKind::PlaylistName:
        case VariableKind::PlaylistSize:
        case VariableKind::PlaylistDuration:
        case VariableKind::ListIndex:
        case VariableKind::Depth:
            return {.sources = {ScriptSources::Playlist, TrackMetadata}, .unclassified = false};
        case VariableKind::LibraryName:
        case VariableKind::LibraryPath:
        case VariableKind::RelativePath:
            return {.sources = {Libraries, TrackMetadata}, .unclassified = false};
        case VariableKind::Bitrate:
            return {.sources = {TrackMetadata, PlaybackOutput, PlaybackTrack}, .unclassified = false};
        case VariableKind::FrontCover:
        case VariableKind::BackCover:
        case VariableKind::ArtistPicture:
            return {.sources = {}, .unclassified = false};
        default:
            return {.sources = {TrackMetadata}, .unclassified = false};
    }
}

} // namespace

class ScriptRegistryPrivate
{
public:
    static constexpr std::uint64_t ShutdownFlag = std::uint64_t{1} << 63;

    void addDefaultVariables();
    void addDefaultFunctions();
    bool registerVariable(ScriptVariableDescriptor variable);
    bool registerFunction(ScriptFunctionDescriptor function);

    bool finalised{false};
    std::atomic_uint64_t operationState{0};
    // Keep this order in sync with ScriptSources
    // Each source ID is its index + 1
    std::vector<QString> dataSources{u"track.metadata"_s, u"playback.track"_s,    u"playback.state"_s,
                                     u"playback.time"_s,  u"playback.output"_s,   u"playback.stopped"_s,
                                     u"playlist"_s,       u"playlist.playback"_s, u"queue"_s,
                                     u"queue.cursor"_s,   u"libraries"_s,         u"clock"_s};
    std::vector<ScriptVariableDescriptor> variables;
    std::vector<ScriptFunctionDescriptor> functions;
    std::unordered_map<QString, size_t> variableIds;
    std::unordered_map<QString, ScriptFunctionId> functionIds;
};

void ScriptRegistryPrivate::addDefaultVariables()
{
    using namespace Constants;

    const auto addVariable = [this](VariableKind kind, QString name, const char* category, const char* description,
                                    ScriptVariableInvoker invoker = {}, std::vector<QString> aliases = {}) {
        const auto dependencies = builtInVariableDependencies(kind, name);
        registerVariable({.kind         = kind,
                          .name         = std::move(name),
                          .invoker      = invoker,
                          .category     = QString::fromUtf8(category),
                          .description  = QString::fromUtf8(description),
                          .aliases      = std::move(aliases),
                          .dependencies = dependencies});
    };

    addVariable(VariableKind::Title, QString::fromLatin1(MetaData::Title), QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                QT_TRANSLATE_NOOP("Fooyin", "Track title"));
    addVariable(VariableKind::Artist, QString::fromLatin1(MetaData::Artist), QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                QT_TRANSLATE_NOOP("Fooyin", "Primary artist"));
    addVariable(VariableKind::TrackArtist, QString::fromLatin1(MetaData::TrackArtist),
                QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                QT_TRANSLATE_NOOP("Fooyin", "Track artists not present in album artists"), {},
                {u"UNIQUEARTIST"_s, u"TRACK ARTIST"_s});
    addVariable(VariableKind::Album, QString::fromLatin1(MetaData::Album), QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                QT_TRANSLATE_NOOP("Fooyin", "Album title"));
    addVariable(VariableKind::AlbumArtist, QString::fromLatin1(MetaData::AlbumArtist),
                QT_TRANSLATE_NOOP("Fooyin", "Metadata"), QT_TRANSLATE_NOOP("Fooyin", "Album artist"), {},
                {u"ALBUM ARTIST"_s});
    addVariable(VariableKind::Track, QString::fromLatin1(MetaData::TrackNumber),
                QT_TRANSLATE_NOOP("Fooyin", "Metadata"), QT_TRANSLATE_NOOP("Fooyin", "Track number"), {},
                {u"TRACKNUMBER"_s, u"TRACK NUMBER"_s});
    addVariable(VariableKind::TrackTotal, QString::fromLatin1(MetaData::TrackTotal),
                QT_TRANSLATE_NOOP("Fooyin", "Metadata"), QT_TRANSLATE_NOOP("Fooyin", "Total tracks on the release"), {},
                {u"TOTALTRACKS"_s});
    addVariable(VariableKind::Disc, QString::fromLatin1(MetaData::Disc), QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                QT_TRANSLATE_NOOP("Fooyin", "Disc number"), {}, {u"DISCNUMBER"_s});
    addVariable(VariableKind::DiscTotal, QString::fromLatin1(MetaData::DiscTotal),
                QT_TRANSLATE_NOOP("Fooyin", "Metadata"), QT_TRANSLATE_NOOP("Fooyin", "Total discs on the release"), {},
                {u"TOTALDISCS"_s});
    addVariable(VariableKind::Genre, QString::fromLatin1(MetaData::Genre), QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                QT_TRANSLATE_NOOP("Fooyin", "Genres"));
    addVariable(VariableKind::Composer, QString::fromLatin1(MetaData::Composer),
                QT_TRANSLATE_NOOP("Fooyin", "Metadata"), QT_TRANSLATE_NOOP("Fooyin", "Composers"));
    addVariable(VariableKind::Performer, QString::fromLatin1(MetaData::Performer),
                QT_TRANSLATE_NOOP("Fooyin", "Metadata"), QT_TRANSLATE_NOOP("Fooyin", "Performers"));
    addVariable(VariableKind::Duration, QString::fromLatin1(MetaData::Duration),
                QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                QT_TRANSLATE_NOOP("Fooyin", "Track duration formatted as time"), {}, {u"LENGTH"_s});
    addVariable(VariableKind::DurationSecs, QString::fromLatin1(MetaData::DurationSecs),
                QT_TRANSLATE_NOOP("Fooyin", "Metadata"), QT_TRANSLATE_NOOP("Fooyin", "Track duration in seconds"), {},
                {u"LENGTH_SECONDS"_s});
    addVariable(VariableKind::DurationMSecs, QString::fromLatin1(MetaData::DurationMSecs),
                QT_TRANSLATE_NOOP("Fooyin", "Metadata"), QT_TRANSLATE_NOOP("Fooyin", "Track duration in milliseconds"));
    addVariable(VariableKind::Comment, QString::fromLatin1(MetaData::Comment), QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                QT_TRANSLATE_NOOP("Fooyin", "Comment tag"));
    addVariable(VariableKind::Date, QString::fromLatin1(MetaData::Date), QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                QT_TRANSLATE_NOOP("Fooyin", "Release date"));
    addVariable(VariableKind::Year, QString::fromLatin1(MetaData::Year), QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                QT_TRANSLATE_NOOP("Fooyin", "Release year"));
    addVariable(VariableKind::FileSize, QString::fromLatin1(MetaData::FileSize),
                QT_TRANSLATE_NOOP("Fooyin", "Metadata"), QT_TRANSLATE_NOOP("Fooyin", "File size in bytes"));
    addVariable(VariableKind::FileSizeNatural, QString::fromLatin1(MetaData::FileSizeNatural),
                QT_TRANSLATE_NOOP("Fooyin", "Metadata"), QT_TRANSLATE_NOOP("Fooyin", "Human readable file size"));
    addVariable(VariableKind::Bitrate, QString::fromLatin1(MetaData::Bitrate), QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                QT_TRANSLATE_NOOP("Fooyin", "Track bitrate"));
    addVariable(VariableKind::SampleRate, QString::fromLatin1(MetaData::SampleRate),
                QT_TRANSLATE_NOOP("Fooyin", "Metadata"), QT_TRANSLATE_NOOP("Fooyin", "Sample rate"));
    addVariable(VariableKind::BitDepth, QString::fromLatin1(MetaData::BitDepth),
                QT_TRANSLATE_NOOP("Fooyin", "Metadata"), QT_TRANSLATE_NOOP("Fooyin", "Bit depth"));
    addVariable(VariableKind::FirstPlayed, QString::fromLatin1(MetaData::FirstPlayed),
                QT_TRANSLATE_NOOP("Fooyin", "Metadata"), QT_TRANSLATE_NOOP("Fooyin", "First played timestamp"));
    addVariable(VariableKind::LastPlayed, QString::fromLatin1(MetaData::LastPlayed),
                QT_TRANSLATE_NOOP("Fooyin", "Metadata"), QT_TRANSLATE_NOOP("Fooyin", "Last played timestamp"));
    addVariable(VariableKind::PlayCount, QString::fromLatin1(MetaData::PlayCount),
                QT_TRANSLATE_NOOP("Fooyin", "Metadata"), QT_TRANSLATE_NOOP("Fooyin", "Play count"), {},
                {u"PLAY_COUNT"_s});
    addVariable(VariableKind::Rating, QString::fromLatin1(MetaData::Rating), QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                QT_TRANSLATE_NOOP("Fooyin", "Numeric rating in stars"));
    addVariable(VariableKind::RatingNormalized, QString::fromLatin1(MetaData::RatingNormalized),
                QT_TRANSLATE_NOOP("Fooyin", "Metadata"), QT_TRANSLATE_NOOP("Fooyin", "Normalised rating"));
    addVariable(VariableKind::Stars, QString::fromLatin1(MetaData::Stars), QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                QT_TRANSLATE_NOOP("Fooyin", "Numeric rating in stars"));
    addVariable(VariableKind::RatingStars, QString::fromLatin1(MetaData::RatingStars),
                QT_TRANSLATE_NOOP("Fooyin", "Metadata"), QT_TRANSLATE_NOOP("Fooyin", "Rating shown as stars"));
    addVariable(VariableKind::RatingStarsPadded, QString::fromLatin1(MetaData::RatingStarsPadded),
                QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                QT_TRANSLATE_NOOP("Fooyin", "Rating shown as stars with trailing empty stars"));
    addVariable(VariableKind::RatingEditor, QString::fromLatin1(MetaData::RatingEditor),
                QT_TRANSLATE_NOOP("Fooyin", "Metadata"), QT_TRANSLATE_NOOP("Fooyin", "Rating editor representation"));
    addVariable(VariableKind::Loved, QString::fromLatin1(MetaData::Loved),
                QT_TRANSLATE_NOOP("Fooyin", "Playback Statistics"),
                QT_TRANSLATE_NOOP("Fooyin", "Love flag: 1 when loved, empty otherwise"));
    addVariable(VariableKind::LoveEditor, QString::fromLatin1(MetaData::LoveEditor),
                QT_TRANSLATE_NOOP("Fooyin", "Playback Statistics"),
                QT_TRANSLATE_NOOP("Fooyin", "Interactive heart editor for the Love flag"));
    addVariable(VariableKind::Codec, QString::fromLatin1(MetaData::Codec), QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                QT_TRANSLATE_NOOP("Fooyin", "Codec name"));
    addVariable(VariableKind::CodecProfile, QString::fromLatin1(MetaData::CodecProfile),
                QT_TRANSLATE_NOOP("Fooyin", "Metadata"), QT_TRANSLATE_NOOP("Fooyin", "Codec profile"));
    addVariable(VariableKind::Tool, QString::fromLatin1(MetaData::Tool), QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                QT_TRANSLATE_NOOP("Fooyin", "Encoding tool"));
    addVariable(VariableKind::TagType, QString::fromLatin1(MetaData::TagType), QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                QT_TRANSLATE_NOOP("Fooyin", "Tag type list"));
    addVariable(VariableKind::Encoding, QString::fromLatin1(MetaData::Encoding),
                QT_TRANSLATE_NOOP("Fooyin", "Metadata"), QT_TRANSLATE_NOOP("Fooyin", "Encoding description"));
    addVariable(VariableKind::Channels, QString::fromLatin1(MetaData::Channels),
                QT_TRANSLATE_NOOP("Fooyin", "Metadata"), QT_TRANSLATE_NOOP("Fooyin", "Channel layout"));
    addVariable(VariableKind::CreatedTime, QString::fromLatin1(MetaData::CreatedTime),
                QT_TRANSLATE_NOOP("Fooyin", "Metadata"), QT_TRANSLATE_NOOP("Fooyin", "File created timestamp"));
    addVariable(VariableKind::AddedTime, QString::fromLatin1(MetaData::AddedTime),
                QT_TRANSLATE_NOOP("Fooyin", "Metadata"), QT_TRANSLATE_NOOP("Fooyin", "Library added timestamp"));
    addVariable(VariableKind::LastModified, QString::fromLatin1(MetaData::LastModified),
                QT_TRANSLATE_NOOP("Fooyin", "Metadata"), QT_TRANSLATE_NOOP("Fooyin", "Last modified timestamp"));
    addVariable(VariableKind::FilePath, QString::fromLatin1(MetaData::FilePath),
                QT_TRANSLATE_NOOP("Fooyin", "Metadata"), QT_TRANSLATE_NOOP("Fooyin", "Full file path"));
    addVariable(VariableKind::RelativePath, QString::fromLatin1(MetaData::RelativePath),
                QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                QT_TRANSLATE_NOOP("Fooyin", "Path relative to the library root"));
    addVariable(VariableKind::FileName, QString::fromLatin1(MetaData::FileName),
                QT_TRANSLATE_NOOP("Fooyin", "Metadata"), QT_TRANSLATE_NOOP("Fooyin", "Filename without extension"));
    addVariable(VariableKind::Extension, QString::fromLatin1(MetaData::Extension),
                QT_TRANSLATE_NOOP("Fooyin", "Metadata"), QT_TRANSLATE_NOOP("Fooyin", "File extension"));
    addVariable(VariableKind::FileNameWithExt, QString::fromLatin1(MetaData::FileNameWithExt),
                QT_TRANSLATE_NOOP("Fooyin", "Metadata"), QT_TRANSLATE_NOOP("Fooyin", "Filename including extension"));
    addVariable(VariableKind::Directory, QString::fromLatin1(MetaData::Directory),
                QT_TRANSLATE_NOOP("Fooyin", "Metadata"), QT_TRANSLATE_NOOP("Fooyin", "Containing directory name"));
    addVariable(VariableKind::Path, QString::fromLatin1(MetaData::Path), QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                QT_TRANSLATE_NOOP("Fooyin", "Containing directory path"));
    addVariable(VariableKind::Subsong, QString::fromLatin1(MetaData::Subsong), QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                QT_TRANSLATE_NOOP("Fooyin", "Subsong index"));
    addVariable(VariableKind::RGTrackGain, QString::fromLatin1(MetaData::RGTrackGain),
                QT_TRANSLATE_NOOP("Fooyin", "Metadata"), QT_TRANSLATE_NOOP("Fooyin", "ReplayGain track gain"));
    addVariable(VariableKind::RGTrackPeak, QString::fromLatin1(MetaData::RGTrackPeak),
                QT_TRANSLATE_NOOP("Fooyin", "Metadata"), QT_TRANSLATE_NOOP("Fooyin", "ReplayGain track peak"));
    addVariable(VariableKind::RGTrackPeakDB, QString::fromLatin1(MetaData::RGTrackPeakDB),
                QT_TRANSLATE_NOOP("Fooyin", "Metadata"), QT_TRANSLATE_NOOP("Fooyin", "ReplayGain track peak in dB"));
    addVariable(VariableKind::RGAlbumGain, QString::fromLatin1(MetaData::RGAlbumGain),
                QT_TRANSLATE_NOOP("Fooyin", "Metadata"), QT_TRANSLATE_NOOP("Fooyin", "ReplayGain album gain"));
    addVariable(VariableKind::RGAlbumPeak, QString::fromLatin1(MetaData::RGAlbumPeak),
                QT_TRANSLATE_NOOP("Fooyin", "Metadata"), QT_TRANSLATE_NOOP("Fooyin", "ReplayGain album peak"));
    addVariable(VariableKind::RGAlbumPeakDB, QString::fromLatin1(MetaData::RGAlbumPeakDB),
                QT_TRANSLATE_NOOP("Fooyin", "Metadata"), QT_TRANSLATE_NOOP("Fooyin", "ReplayGain album peak in dB"));
    addVariable(VariableKind::TrackCount, u"TRACKCOUNT"_s, QT_TRANSLATE_NOOP("Fooyin", "Playlist"),
                QT_TRANSLATE_NOOP("Fooyin", "Number of tracks in the list"));
    addVariable(VariableKind::Playtime, u"PLAYTIME"_s, QT_TRANSLATE_NOOP("Fooyin", "Playlist"),
                QT_TRANSLATE_NOOP("Fooyin", "Combined duration of the track list"));
    addVariable(VariableKind::AverageBitrate, u"AVGBITRATE"_s, QT_TRANSLATE_NOOP("Fooyin", "Playlist"),
                QT_TRANSLATE_NOOP("Fooyin", "Duration-weighted average bitrate of the track list in kbps"));
    addVariable(VariableKind::PlaylistName, u"PLAYLIST_NAME"_s, QT_TRANSLATE_NOOP("Fooyin", "Playlist"),
                QT_TRANSLATE_NOOP("Fooyin", "Name of the playlist"));
    addVariable(VariableKind::PlaylistSize, u"PLAYLIST_SIZE"_s, QT_TRANSLATE_NOOP("Fooyin", "Playlist"),
                QT_TRANSLATE_NOOP("Fooyin", "Combined file size of the track list"));
    addVariable(VariableKind::PlaylistDuration, u"PLAYLIST_DURATION"_s, QT_TRANSLATE_NOOP("Fooyin", "Playlist"),
                QT_TRANSLATE_NOOP("Fooyin", "Alias for total playlist duration"));
    addVariable(VariableKind::PlaylistElapsed, u"PLAYLIST_ELAPSED"_s, QT_TRANSLATE_NOOP("Fooyin", "Playback"),
                QT_TRANSLATE_NOOP("Fooyin", "Elapsed time within the active playlist"));
    addVariable(VariableKind::Genres, u"GENRES"_s, QT_TRANSLATE_NOOP("Fooyin", "Playlist"),
                QT_TRANSLATE_NOOP("Fooyin", "Unique genres across the track list"));
    addVariable(VariableKind::PlaybackTime, u"PLAYBACK_TIME"_s, QT_TRANSLATE_NOOP("Fooyin", "Playback"),
                QT_TRANSLATE_NOOP("Fooyin", "Current playback position formatted as time"));
    addVariable(VariableKind::PlaybackTimeSeconds, u"PLAYBACK_TIME_S"_s, QT_TRANSLATE_NOOP("Fooyin", "Playback"),
                QT_TRANSLATE_NOOP("Fooyin", "Current playback position in seconds"), {}, {u"PLAYBACK_TIME_SECONDS"_s});
    addVariable(VariableKind::PlaybackTimeRemaining, u"PLAYBACK_TIME_REMAINING"_s,
                QT_TRANSLATE_NOOP("Fooyin", "Playback"),
                QT_TRANSLATE_NOOP("Fooyin", "Remaining playback time formatted as time"));
    addVariable(VariableKind::PlaybackTimeRemainingSeconds, u"PLAYBACK_TIME_REMAINING_S"_s,
                QT_TRANSLATE_NOOP("Fooyin", "Playback"),
                QT_TRANSLATE_NOOP("Fooyin", "Remaining playback time in seconds"), {},
                {u"PLAYBACK_TIME_REMAINING_SECONDS"_s});
    addVariable(VariableKind::InputDecoder, u"INPUT_DECODER"_s, QT_TRANSLATE_NOOP("Fooyin", "Playback"),
                QT_TRANSLATE_NOOP("Fooyin", "Input decoder backend used for the current playing track"));
    addVariable(VariableKind::OutputSampleRate, u"OUTPUT_SAMPLERATE"_s, QT_TRANSLATE_NOOP("Fooyin", "Playback"),
                QT_TRANSLATE_NOOP("Fooyin", "Sample rate entering the active output"));
    addVariable(VariableKind::OutputChannels, u"OUTPUT_CHANNELS"_s, QT_TRANSLATE_NOOP("Fooyin", "Playback"),
                QT_TRANSLATE_NOOP("Fooyin", "Number of channels entering the active output"));
    addVariable(VariableKind::OutputChannelMask, u"OUTPUT_CHANNEL_MASK"_s, QT_TRANSLATE_NOOP("Fooyin", "Playback"),
                QT_TRANSLATE_NOOP("Fooyin", "Channel layout entering the active output"));
    addVariable(VariableKind::OutputBitDepth, u"OUTPUT_BITDEPTH"_s, QT_TRANSLATE_NOOP("Fooyin", "Playback"),
                QT_TRANSLATE_NOOP("Fooyin", "Bit depth used by the active output"));
    addVariable(VariableKind::OutputDevice, u"OUTPUT_DEVICE"_s, QT_TRANSLATE_NOOP("Fooyin", "Playback"),
                QT_TRANSLATE_NOOP("Fooyin", "Active output device"));
    addVariable(VariableKind::OutputDsps, u"OUTPUT_DSPS"_s, QT_TRANSLATE_NOOP("Fooyin", "Playback"),
                QT_TRANSLATE_NOOP("Fooyin", "Enabled DSPs in processing order"));
    addVariable(VariableKind::OutputDspPreset, u"OUTPUT_DSP_PRESET"_s, QT_TRANSLATE_NOOP("Fooyin", "Playback"),
                QT_TRANSLATE_NOOP("Fooyin", "Explicitly selected DSP chain preset"));
    addVariable(VariableKind::OutputVolume, u"OUTPUT_VOLUME"_s, QT_TRANSLATE_NOOP("Fooyin", "Playback"),
                QT_TRANSLATE_NOOP("Fooyin", "Playback volume in dB"));
    addVariable(VariableKind::OutputRgSource, u"OUTPUT_RG_SOURCE"_s, QT_TRANSLATE_NOOP("Fooyin", "Playback"),
                QT_TRANSLATE_NOOP("Fooyin", "Active ReplayGain source mode"));
    addVariable(VariableKind::OutputRgMode, u"OUTPUT_RG_MODE"_s, QT_TRANSLATE_NOOP("Fooyin", "Playback"),
                QT_TRANSLATE_NOOP("Fooyin", "Active ReplayGain processing mode"));
    addVariable(VariableKind::OutputRgGain, u"OUTPUT_RG_GAIN"_s, QT_TRANSLATE_NOOP("Fooyin", "Playback"),
                QT_TRANSLATE_NOOP("Fooyin", "Effective ReplayGain adjustment in dB"));
    addVariable(VariableKind::OutputRgPeak, u"OUTPUT_RG_PEAK"_s, QT_TRANSLATE_NOOP("Fooyin", "Playback"),
                QT_TRANSLATE_NOOP("Fooyin", "Effective ReplayGain peak"));
    addVariable(VariableKind::OutputRgPeakDb, u"OUTPUT_RG_PEAK_DB"_s, QT_TRANSLATE_NOOP("Fooyin", "Playback"),
                QT_TRANSLATE_NOOP("Fooyin", "Effective ReplayGain peak in dBFS"));
    addVariable(VariableKind::OutputBufferLength, u"OUTPUT_BUFFER_LENGTH"_s, QT_TRANSLATE_NOOP("Fooyin", "Playback"),
                QT_TRANSLATE_NOOP("Fooyin", "Active output buffer length in milliseconds"));
    addVariable(VariableKind::IsPlaying, u"ISPLAYING"_s, QT_TRANSLATE_NOOP("Fooyin", "Playback"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns 1 while playback is active"));
    addVariable(VariableKind::IsPaused, u"ISPAUSED"_s, QT_TRANSLATE_NOOP("Fooyin", "Playback"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns 1 while playback is paused"));
    addVariable(VariableKind::IsStopped, u"ISSTOPPED"_s, QT_TRANSLATE_NOOP("Fooyin", "Playback"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns 1 while playback is stopped"));
    addVariable(VariableKind::IsQueueCurrent, u"IS_QUEUE_CURRENT"_s, QT_TRANSLATE_NOOP("Fooyin", "Queue"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns 1 for the current queue viewer entry, including while paused"),
                makeScriptVariableInvoker<queueItemState<ScriptQueueItemState::Current>>());
    addVariable(VariableKind::Generic, u"_FOOYIN_VERSION"_s, QT_TRANSLATE_NOOP("Fooyin", "System"),
                QT_TRANSLATE_NOOP("Fooyin", "fooyin version"), makeScriptVariableInvoker<fooyinVersionVariable>());
    addVariable(VariableKind::Generic, u"DATETIME"_s, QT_TRANSLATE_NOOP("Fooyin", "System"),
                QT_TRANSLATE_NOOP("Fooyin", "Current date and time formatted as YYYY-MM-DD HH:MM:SS"),
                makeScriptVariableInvoker<dateTimeVariable>());
    addVariable(VariableKind::LibraryName, u"LIBRARYNAME"_s, QT_TRANSLATE_NOOP("Fooyin", "Library"),
                QT_TRANSLATE_NOOP("Fooyin", "Current library name"));
    addVariable(VariableKind::LibraryPath, u"LIBRARYPATH"_s, QT_TRANSLATE_NOOP("Fooyin", "Library"),
                QT_TRANSLATE_NOOP("Fooyin", "Current library path"));
    addVariable(VariableKind::QueueIndex, u"QUEUE_INDEX"_s, QT_TRANSLATE_NOOP("Fooyin", "Queue"),
                QT_TRANSLATE_NOOP("Fooyin", "First playback queue index for the specified item"),
                makeScriptVariableInvoker<queueIndex>(), {u"QUEUEINDEX"_s});
    addVariable(VariableKind::QueueMode, u"QUEUE_MODE"_s, QT_TRANSLATE_NOOP("Fooyin", "Queue"),
                QT_TRANSLATE_NOOP("Fooyin", "Playback queue mode: playlist or queue"),
                makeScriptVariableInvoker<queueMode>(), {u"QUEUEMODE"_s});
    addVariable(VariableKind::QueueIndexes, u"QUEUE_INDEXES"_s, QT_TRANSLATE_NOOP("Fooyin", "Queue"),
                QT_TRANSLATE_NOOP("Fooyin", "Playback queue indexes for the specified item"),
                makeScriptVariableInvoker<queueIndexes>(), {u"QUEUEINDEXES"_s});
    addVariable(VariableKind::QueueTotal, u"QUEUE_TOTAL"_s, QT_TRANSLATE_NOOP("Fooyin", "Queue"),
                QT_TRANSLATE_NOOP("Fooyin", "Total amount of tracks in the playback queue for queued items"),
                makeScriptVariableInvoker<queueTotal>(), {u"QUEUETOTAL"_s});
    addVariable(VariableKind::IsQueueUpcoming, u"IS_QUEUE_UPCOMING"_s, QT_TRANSLATE_NOOP("Fooyin", "Queue"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns 1 for a queue viewer entry ahead of the queue cursor"),
                makeScriptVariableInvoker<queueItemState<ScriptQueueItemState::Upcoming>>());
    addVariable(VariableKind::IsQueuePlayed, u"IS_QUEUE_PLAYED"_s, QT_TRANSLATE_NOOP("Fooyin", "Queue"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns 1 for a queue viewer entry in queue history"),
                makeScriptVariableInvoker<queueItemState<ScriptQueueItemState::Played>>());
    addVariable(VariableKind::ListIndex, u"LIST_INDEX"_s, QT_TRANSLATE_NOOP("Fooyin", "Playlist"),
                QT_TRANSLATE_NOOP("Fooyin", "One-based index of the evaluated playlist row"),
                makeScriptVariableInvoker<trackIndex>());
    addVariable(VariableKind::Depth, u"DEPTH"_s, QT_TRANSLATE_NOOP("Fooyin", "Playlist"),
                QT_TRANSLATE_NOOP("Fooyin", "Grouping depth of the evaluated playlist row"),
                makeScriptVariableInvoker<trackDepth>());
    addVariable(VariableKind::PlayingIcon, u"PLAYINGICON"_s, QT_TRANSLATE_NOOP("Fooyin", "Playlist"),
                QT_TRANSLATE_NOOP("Fooyin", "Playback icon marker in supported views"),
                makeScriptVariableInvoker<playingIcon>());
    addVariable(VariableKind::FrontCover, u"FRONTCOVER"_s, QT_TRANSLATE_NOOP("Fooyin", "Artwork"),
                QT_TRANSLATE_NOOP("Fooyin", "Front cover marker in supported views"),
                makeScriptVariableInvoker<frontCover>());
    addVariable(VariableKind::BackCover, u"BACKCOVER"_s, QT_TRANSLATE_NOOP("Fooyin", "Artwork"),
                QT_TRANSLATE_NOOP("Fooyin", "Back cover marker in supported views"),
                makeScriptVariableInvoker<backCover>());
    addVariable(VariableKind::ArtistPicture, u"ARTISTPICTURE"_s, QT_TRANSLATE_NOOP("Fooyin", "Artwork"),
                QT_TRANSLATE_NOOP("Fooyin", "Artist picture marker in supported views"),
                makeScriptVariableInvoker<artistPicture>());
}

void ScriptRegistryPrivate::addDefaultFunctions()
{
    const auto addFunction = [this](QString name, QString signature, const char* category, const char* description,
                                    ScriptFunctionInvoker invoker = {}, FunctionKind kind = FunctionKind::Generic,
                                    std::vector<QString> aliases = {}) {
        ScriptDependencies dependencies{.unclassified = false};
        if(name == "meta"_L1 || name == "meta_sep"_L1 || name == "meta_test"_L1 || name == "meta_num"_L1
           || name == "info"_L1) {
            dependencies.sources.push_back(ScriptSources::TrackMetadata);
        }

        registerFunction({.name         = std::move(name),
                          .invoker      = invoker,
                          .signature    = std::move(signature),
                          .category     = QString::fromUtf8(category),
                          .description  = QString::fromUtf8(description),
                          .kind         = kind,
                          .aliases      = std::move(aliases),
                          .dependencies = std::move(dependencies)});
    };

    addFunction(u"add"_s, u"$add(x,y,…)"_s, QT_TRANSLATE_NOOP("Fooyin", "Numeric"),
                QT_TRANSLATE_NOOP("Fooyin", "Adds numeric arguments"), {}, FunctionKind::Add);
    addFunction(u"sub"_s, u"$sub(x,y,…)"_s, QT_TRANSLATE_NOOP("Fooyin", "Numeric"),
                QT_TRANSLATE_NOOP("Fooyin", "Subtracts later values from the first"), {}, FunctionKind::Sub);
    addFunction(u"mul"_s, u"$mul(x,y,…)"_s, QT_TRANSLATE_NOOP("Fooyin", "Numeric"),
                QT_TRANSLATE_NOOP("Fooyin", "Multiplies numeric arguments"), {}, FunctionKind::Mul);
    addFunction(u"div"_s, u"$div(x,y)"_s, QT_TRANSLATE_NOOP("Fooyin", "Numeric"),
                QT_TRANSLATE_NOOP("Fooyin", "Divides the first value by the second"), {}, FunctionKind::Div);
    addFunction(u"min"_s, u"$min(x,y,…)"_s, QT_TRANSLATE_NOOP("Fooyin", "Numeric"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns the smallest numeric value"),
                makeScriptFunctionInvoker<Scripting::min>());
    addFunction(u"max"_s, u"$max(x,y,…)"_s, QT_TRANSLATE_NOOP("Fooyin", "Numeric"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns the largest numeric value"),
                makeScriptFunctionInvoker<Scripting::max>());
    addFunction(u"mod"_s, u"$mod(x,y)"_s, QT_TRANSLATE_NOOP("Fooyin", "Numeric"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns the remainder of a division"), {}, FunctionKind::Mod);
    addFunction(u"rand"_s, u"$rand(min,max)"_s, QT_TRANSLATE_NOOP("Fooyin", "Numeric"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns a random number in range"),
                makeScriptFunctionInvoker<Scripting::rand>());
    addFunction(u"round"_s, u"$round(value[,precision])"_s, QT_TRANSLATE_NOOP("Fooyin", "Numeric"),
                QT_TRANSLATE_NOOP("Fooyin", "Rounds a numeric value"), makeScriptFunctionInvoker<Scripting::round>());
    addFunction(u"greater"_s, u"$greater(x,y)"_s, QT_TRANSLATE_NOOP("Fooyin", "Conditional"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns true when x is greater than y"),
                makeScriptFunctionInvoker<Scripting::greater>());
    addFunction(u"num"_s, u"$num(value,length)"_s, QT_TRANSLATE_NOOP("Fooyin", "Numeric"),
                QT_TRANSLATE_NOOP("Fooyin", "Formats a number with leading zeroes"), {}, FunctionKind::Num);
    addFunction(u"hex"_s, u"$hex(value,length)"_s, QT_TRANSLATE_NOOP("Fooyin", "Numeric"),
                QT_TRANSLATE_NOOP("Fooyin", "Formats a number in hexadecimal with leading zeroes"),
                makeScriptFunctionInvoker<Scripting::hex>());
    addFunction(u"replace"_s, u"$replace(text,from,to,…)"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Replaces text fragments"),
                makeScriptFunctionInvoker<Scripting::replace>());
    addFunction(u"regex_match"_s, u"$regex_match(text,\"pattern\"[,group[,flags]])"_s,
                QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns the first match or capture; group 0 is the full match"),
                makeScriptFunctionInvoker<Scripting::regexMatch>());
    addFunction(u"regex_matches"_s, u"$regex_matches(text,\"pattern\",separator[,group[,flags]])"_s,
                QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns all matches or captures; group 0 is the full match"),
                makeScriptFunctionInvoker<Scripting::regexMatches>());
    addFunction(u"regex_replace"_s, u"$regex_replace(text,\"pattern\",replacement[,flags])"_s,
                QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Replaces all regular expression matches; flags: i, m, s, x, U"),
                makeScriptFunctionInvoker<Scripting::regexReplace>());
    addFunction(u"ascii"_s, u"$ascii(text)"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Converts text to ASCII"), makeScriptFunctionInvoker<Scripting::ascii>());
    addFunction(u"slice"_s, u"$slice(text,start[,end])"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns a slice of text"), makeScriptFunctionInvoker<Scripting::slice>());
    addFunction(u"chop"_s, u"$chop(text,count)"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Removes characters from the end"),
                makeScriptFunctionInvoker<Scripting::chop>());
    addFunction(u"left"_s, u"$left(text,count)"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns characters from the left"),
                makeScriptFunctionInvoker<Scripting::left>(), FunctionKind::Generic, {u"cut"_s});
    addFunction(u"right"_s, u"$right(text,count)"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns characters from the right"),
                makeScriptFunctionInvoker<Scripting::right>());
    addFunction(u"insert"_s, u"$insert(text,insert[,pos])"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Inserts text at a position"),
                makeScriptFunctionInvoker<Scripting::insert>());
    addFunction(u"substr"_s, u"$substr(text,start,end)"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns a substring"), makeScriptFunctionInvoker<Scripting::substr>());
    addFunction(u"strstr"_s, u"$strstr(text,needle[,start])"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Finds the first substring position (1-based)"),
                makeScriptFunctionInvoker<Scripting::strstr>());
    addFunction(u"stristr"_s, u"$stristr(text,needle[,start])"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Finds the first substring position ignoring case (1-based)"),
                makeScriptFunctionInvoker<Scripting::stristr>());
    addFunction(u"strstrlast"_s, u"$strstrlast(text,needle[,start])"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Finds the last substring position (1-based)"),
                makeScriptFunctionInvoker<Scripting::strstrLast>());
    addFunction(u"stristrlast"_s, u"$stristrlast(text,needle[,start])"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Finds the last substring position ignoring case (1-based)"),
                makeScriptFunctionInvoker<Scripting::stristrLast>());
    addFunction(u"split"_s, u"$split(text,sep[,index])"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Splits text into multiple values, or returns one split segment "
                                            "when given a 1-based index"),
                makeScriptFunctionInvoker<Scripting::split>());
    addFunction(u"join"_s, u"$join(sep,value,…)"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Joins non-empty values with a separator"),
                makeScriptFunctionInvoker<Scripting::join>());
    addFunction(u"len"_s, u"$len(text)"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns the text length"), makeScriptFunctionInvoker<Scripting::len>());
    addFunction(u"longest"_s, u"$longest(a,b,…)"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns the longest string"),
                makeScriptFunctionInvoker<Scripting::longest>());
    addFunction(u"shortest"_s, u"$shortest(a,b,…)"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns the first shortest string"),
                makeScriptFunctionInvoker<Scripting::shortest>());
    addFunction(u"strchr"_s, u"$strchr(text,char)"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Finds the first character position (1-based)"),
                makeScriptFunctionInvoker<Scripting::strchr>());
    addFunction(u"strrchr"_s, u"$strrchr(text,char)"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Finds the last character position (1-based)"),
                makeScriptFunctionInvoker<Scripting::strrchr>());
    addFunction(u"strcmp"_s, u"$strcmp(a,b)"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Compares two strings"), makeScriptFunctionInvoker<Scripting::strcmp>());
    addFunction(u"stricmp"_s, u"$stricmp(a,b)"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Compares two strings ignoring case"),
                makeScriptFunctionInvoker<Scripting::stricmp>());
    addFunction(u"regex_test"_s, u"$regex_test(text,\"pattern\"[,flags])"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Tests a regular expression match; flags: i, m, s, x, U"),
                makeScriptFunctionInvoker<Scripting::regexTest>());
    addFunction(u"longer"_s, u"$longer(a,b)"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns true when a is longer than b"),
                makeScriptFunctionInvoker<Scripting::longer>());
    addFunction(u"sep"_s, u"$sep()"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns the platform path separator"),
                makeScriptFunctionInvoker<Scripting::sep>());
    addFunction(u"crlf"_s, u"$crlf()"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns a newline"), makeScriptFunctionInvoker<Scripting::crlf>());
    addFunction(u"tab"_s, u"$tab()"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns a tab character"), makeScriptFunctionInvoker<Scripting::tab>());
    addFunction(u"swapprefix"_s, u"$swapprefix(text)"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Moves leading articles to the end"),
                makeScriptFunctionInvoker<Scripting::swapPrefix>());
    addFunction(u"stripprefix"_s, u"$stripprefix(text)"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Removes leading articles"),
                makeScriptFunctionInvoker<Scripting::stripPrefix>());
    addFunction(u"pad"_s, u"$pad(text,length[,char])"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Pads text on the left"), {}, FunctionKind::Pad);
    addFunction(u"padright"_s, u"$padright(text,length[,char])"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Pads text on the right"), {}, FunctionKind::PadRight);
    addFunction(u"repeat"_s, u"$repeat(text,count)"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Repeats text"), makeScriptFunctionInvoker<Scripting::repeat>());
    addFunction(u"trim"_s, u"$trim(text)"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Trims surrounding whitespace"),
                makeScriptFunctionInvoker<Scripting::trim>());
    addFunction(u"lower"_s, u"$lower(text)"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Converts text to lowercase"),
                makeScriptFunctionInvoker<Scripting::lower>());
    addFunction(u"upper"_s, u"$upper(text)"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Converts text to uppercase"),
                makeScriptFunctionInvoker<Scripting::upper>());
    addFunction(u"abbr"_s, u"$abbr(text)"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Builds an abbreviation"), makeScriptFunctionInvoker<Scripting::abbr>());
    addFunction(u"caps"_s, u"$caps(text)"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Capitalises words"), makeScriptFunctionInvoker<Scripting::caps>());
    addFunction(u"directory"_s, u"$directory(path,level)"_s, QT_TRANSLATE_NOOP("Fooyin", "Path"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns a directory name from a path"),
                makeScriptFunctionInvoker<Scripting::directory>());
    addFunction(u"directory_path"_s, u"$directory_path(path,level)"_s, QT_TRANSLATE_NOOP("Fooyin", "Path"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns a parent directory path"),
                makeScriptFunctionInvoker<Scripting::directoryPath>());
    addFunction(u"elide_end"_s, u"$elide_end(text,width[,delim])"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Elides text at the end"),
                makeScriptFunctionInvoker<Scripting::elideEnd>());
    addFunction(u"elide_mid"_s, u"$elide_mid(text,width[,delim])"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Elides text in the middle"),
                makeScriptFunctionInvoker<Scripting::elideMid>());
    addFunction(u"ext"_s, u"$ext(path)"_s, QT_TRANSLATE_NOOP("Fooyin", "Path"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns a file extension"), makeScriptFunctionInvoker<Scripting::ext>());
    addFunction(u"filename"_s, u"$filename(path)"_s, QT_TRANSLATE_NOOP("Fooyin", "Path"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns a filename without extension"),
                makeScriptFunctionInvoker<Scripting::filename>());
    addFunction(u"progress"_s, u"$progress(pos,total,length)"_s, QT_TRANSLATE_NOOP("Fooyin", "Utility"),
                QT_TRANSLATE_NOOP("Fooyin", "Builds a text progress bar"),
                makeScriptFunctionInvoker<Scripting::progress>());
    addFunction(u"progress2"_s, u"$progress2(pos,total,length)"_s, QT_TRANSLATE_NOOP("Fooyin", "Utility"),
                QT_TRANSLATE_NOOP("Fooyin", "Builds an alternate text progress bar"),
                makeScriptFunctionInvoker<Scripting::progress2>());
    addFunction(u"doclink"_s, u"$doclink(label,url)"_s, QT_TRANSLATE_NOOP("Fooyin", "Utility"),
                QT_TRANSLATE_NOOP("Fooyin", "Builds a clickable document or web link"),
                makeScriptFunctionInvoker<Scripting::doclink>());
    addFunction(u"cmdlink"_s, u"$cmdlink(label,id)"_s, QT_TRANSLATE_NOOP("Fooyin", "Utility"),
                QT_TRANSLATE_NOOP("Fooyin", "Builds a clickable link to a fooyin command"),
                makeScriptFunctionInvoker<Scripting::cmdlink>());
    addFunction(u"applink"_s, u"$applink(label,application[,arguments[,directory]])"_s,
                QT_TRANSLATE_NOOP("Fooyin", "Utility"),
                QT_TRANSLATE_NOOP("Fooyin", "Builds a clickable link that launches an application"),
                makeScriptFunctionInvoker<Scripting::applink>());
    addFunction(u"urlencode"_s, u"$urlencode(text)"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Percent-encodes text for use in URLs"),
                makeScriptFunctionInvoker<Scripting::urlencode>());
    addFunction(u"crc32"_s, u"$crc32(text)"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns the CRC-32 checksum of the text"),
                makeScriptFunctionInvoker<Scripting::crc32>());
    addFunction(u"isalpha"_s, u"$isalpha(text)"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Checks if text contains only alphabetic characters"),
                makeScriptFunctionInvoker<Scripting::isalpha>());
    addFunction(u"isalnum"_s, u"$isalnum(text)"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Checks if text contains only alphanumeric characters"),
                makeScriptFunctionInvoker<Scripting::isalnum>());
    addFunction(u"isnum"_s, u"$isnum(text)"_s, QT_TRANSLATE_NOOP("Fooyin", "String"),
                QT_TRANSLATE_NOOP("Fooyin", "Checks if text contains only numeric characters"),
                makeScriptFunctionInvoker<Scripting::isnum>());
    addFunction(u"timems"_s, u"$timems(milliseconds)"_s, QT_TRANSLATE_NOOP("Fooyin", "Numeric"),
                QT_TRANSLATE_NOOP("Fooyin", "Formats milliseconds as time"),
                makeScriptFunctionInvoker<Scripting::msToString>());
    addFunction(u"year"_s, u"$year(time)"_s, QT_TRANSLATE_NOOP("Fooyin", "Time"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns the four-digit year from a date"),
                makeScriptFunctionInvoker<Scripting::year>());
    addFunction(u"month"_s, u"$month(time)"_s, QT_TRANSLATE_NOOP("Fooyin", "Time"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns the two-digit month from a date"),
                makeScriptFunctionInvoker<Scripting::month>());
    addFunction(u"day_of_month"_s, u"$day_of_month(time)"_s, QT_TRANSLATE_NOOP("Fooyin", "Time"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns the two-digit day of month from a date"),
                makeScriptFunctionInvoker<Scripting::dayOfMonth>());
    addFunction(u"date"_s, u"$date(time)"_s, QT_TRANSLATE_NOOP("Fooyin", "Time"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns the date formatted as YYYY-MM-DD"),
                makeScriptFunctionInvoker<Scripting::date>());
    addFunction(u"time"_s, u"$time(time)"_s, QT_TRANSLATE_NOOP("Fooyin", "Time"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns the time formatted as HH:MM or HH:MM:SS"),
                makeScriptFunctionInvoker<Scripting::time>());
    addFunction(u"get"_s, u"$get(name)"_s, QT_TRANSLATE_NOOP("Fooyin", "Variable"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns the value stored in a script variable"), {}, FunctionKind::Get);
    addFunction(u"put"_s, u"$put(name,value)"_s, QT_TRANSLATE_NOOP("Fooyin", "Variable"),
                QT_TRANSLATE_NOOP("Fooyin", "Stores a script variable and returns the value"), {}, FunctionKind::Put);
    addFunction(u"puts"_s, u"$puts(name,value)"_s, QT_TRANSLATE_NOOP("Fooyin", "Variable"),
                QT_TRANSLATE_NOOP("Fooyin", "Stores a script variable and returns nothing"), {}, FunctionKind::Puts);
    addFunction(u"and"_s, u"$and(expr,…)"_s, QT_TRANSLATE_NOOP("Fooyin", "Conditional"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns true when all expressions are true"),
                makeScriptFunctionInvoker<Scripting::boolAnd>());
    addFunction(u"not"_s, u"$not(expr)"_s, QT_TRANSLATE_NOOP("Fooyin", "Conditional"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns the opposite truth value"),
                makeScriptFunctionInvoker<Scripting::boolNot>());
    addFunction(u"or"_s, u"$or(expr,…)"_s, QT_TRANSLATE_NOOP("Fooyin", "Conditional"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns true when at least one expression is true"),
                makeScriptFunctionInvoker<Scripting::boolOr>());
    addFunction(u"xor"_s, u"$xor(expr,…)"_s, QT_TRANSLATE_NOOP("Fooyin", "Conditional"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns true when an odd number of expressions are true"),
                makeScriptFunctionInvoker<Scripting::boolXOr>());
    addFunction(u"if"_s, u"$if(condition,then[,else])"_s, QT_TRANSLATE_NOOP("Fooyin", "Conditional"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns then when condition is true"), {}, FunctionKind::If);
    addFunction(u"if2"_s, u"$if2(value[,fallback])"_s, QT_TRANSLATE_NOOP("Fooyin", "Conditional"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns the first non-empty value"), {}, FunctionKind::If2);
    addFunction(u"if3"_s, u"$if3(a1,a2,…,aN,else)"_s, QT_TRANSLATE_NOOP("Fooyin", "Conditional"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns the first true value from a list, or else when none match"), {},
                FunctionKind::If3);
    addFunction(u"select"_s, u"$select(index,a1,…,aN)"_s, QT_TRANSLATE_NOOP("Fooyin", "Conditional"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns the value at the one-based index"), {}, FunctionKind::Select);
    addFunction(u"ifgreater"_s, u"$ifgreater(x,y,then,else)"_s, QT_TRANSLATE_NOOP("Fooyin", "Conditional"),
                QT_TRANSLATE_NOOP("Fooyin", "Compares numeric values"), {}, FunctionKind::IfGreater);
    addFunction(u"iflonger"_s, u"$iflonger(text,length,then,else)"_s, QT_TRANSLATE_NOOP("Fooyin", "Conditional"),
                QT_TRANSLATE_NOOP("Fooyin", "Checks whether text is longer than a limit"), {}, FunctionKind::IfLonger);
    addFunction(u"ifequal"_s, u"$ifequal(x,y,then,else)"_s, QT_TRANSLATE_NOOP("Fooyin", "Conditional"),
                QT_TRANSLATE_NOOP("Fooyin", "Checks whether two numeric values are equal"), {}, FunctionKind::IfEqual);
    addFunction(u"meta"_s, u"$meta(field[,index])"_s, QT_TRANSLATE_NOOP("Fooyin", "Lookup"),
                QT_TRANSLATE_NOOP("Fooyin", "Looks up a raw tag field. Multiple values are joined with ‘, ’ "
                                            "or selected by the optional zero-based index."),
                makeScriptFunctionInvoker<trackMeta>());
    addFunction(u"meta_sep"_s, u"$meta_sep(field,sep[,lastsep])"_s, QT_TRANSLATE_NOOP("Fooyin", "Lookup"),
                QT_TRANSLATE_NOOP("Fooyin", "Looks up a raw tag field. Multiple values are joined with sep, "
                                            "using lastsep between the final two values when supplied."),
                makeScriptFunctionInvoker<trackMetaSep>());
    addFunction(u"meta_test"_s, u"$meta_test(field,…)"_s, QT_TRANSLATE_NOOP("Fooyin", "Lookup"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns 1 when all named tag fields exist."),
                makeScriptFunctionInvoker<trackMetaTest>());
    addFunction(u"meta_num"_s, u"$meta_num(field)"_s, QT_TRANSLATE_NOOP("Fooyin", "Lookup"),
                QT_TRANSLATE_NOOP("Fooyin", "Returns the number of values in a raw tag field."),
                makeScriptFunctionInvoker<trackMetaNum>());
    addFunction(u"info"_s, u"$info(field)"_s, QT_TRANSLATE_NOOP("Fooyin", "Lookup"),
                QT_TRANSLATE_NOOP("Fooyin", "Looks up technical track information"),
                makeScriptFunctionInvoker<trackInfo>());
}

bool ScriptRegistryPrivate::registerVariable(ScriptVariableDescriptor variable)
{
    if(finalised) {
        qWarning() << "Cannot register script variable:" << variable.name << "registry is finalised";
        return false;
    }

    variable.name = variable.name.toUpper();

    std::unordered_set names{variable.name};
    for(auto& alias : variable.aliases) {
        alias = alias.toUpper();
        names.insert(alias);
    }

    if(names.size() != variable.aliases.size() + 1 || std::ranges::any_of(names, [this](const auto& name) {
           return name.isEmpty() || variableIds.contains(name);
       })) {
        qWarning() << "Cannot register script variable:" << variable.name << "empty or duplicate name";
        return false;
    }

    for(const auto& name : names) {
        variableIds.emplace(name, variables.size());
    }

    variables.push_back(std::move(variable));
    return true;
}

bool ScriptRegistryPrivate::registerFunction(ScriptFunctionDescriptor function)
{
    if(finalised) {
        qWarning() << "Cannot register script function:" << function.name << "registry is finalised";
        return false;
    }

    function.name = function.name.toLower();

    std::unordered_set names{function.name};
    for(auto& alias : function.aliases) {
        alias = alias.toLower();
        names.insert(alias);
    }

    if(names.size() != function.aliases.size() + 1 || std::ranges::any_of(names, [this](const auto& name) {
           return name.isEmpty() || functionIds.contains(name);
       })) {
        qWarning() << "Cannot register script function:" << function.name << "empty or duplicate name";
        return false;
    }

    const auto id = static_cast<ScriptFunctionId>(functions.size() + 1);
    for(const auto& name : names) {
        functionIds.emplace(name, id);
    }

    functions.push_back(std::move(function));
    return true;
}

ScriptRegistry::ScriptRegistry()
    : p{std::make_unique<ScriptRegistryPrivate>()}
{
    p->addDefaultFunctions();
    p->addDefaultVariables();
}

ScriptRegistry::~ScriptRegistry() = default;

ScriptRegistry& ScriptRegistry::instance()
{
    static ScriptRegistry registry;
    return registry;
}

ScriptDataSourceId ScriptRegistry::registerDataSource(QString name)
{
    if(p->finalised || name.isEmpty() || std::ranges::find(p->dataSources, name) != p->dataSources.end()) {
        qWarning() << "Cannot register script data source:" << name;
        return InvalidScriptDataSource;
    }

    p->dataSources.push_back(std::move(name));
    return static_cast<ScriptDataSourceId>(p->dataSources.size());
}

size_t ScriptRegistry::dataSourceCount() const
{
    return p->dataSources.size();
}

bool ScriptRegistry::registerVariable(ScriptVariableDescriptor variable)
{
    const bool hasBuiltInPath = variable.kind != VariableKind::Generic
                             && std::ranges::any_of(p->variables, [&variable](const auto& existing) {
                                    return existing.kind == variable.kind && !existing.invoker;
                                });
    if(!variable.invoker && !variable.contextual && !hasBuiltInPath) {
        qWarning() << "Cannot register script variable:" << variable.name << "invalid invoker";
        return false;
    }
    return p->registerVariable(std::move(variable));
}

bool ScriptRegistry::registerFunction(ScriptFunctionDescriptor function)
{
    const bool hasBuiltInPath = function.kind != FunctionKind::Generic
                             && std::ranges::any_of(p->functions, [&function](const auto& existing) {
                                    return existing.kind == function.kind && !existing.invoker;
                                });
    if(!function.invoker && !hasBuiltInPath) {
        qWarning() << "Cannot register script function:" << function.name << "invalid invoker";
        return false;
    }
    return p->registerFunction(std::move(function));
}

void ScriptRegistry::finalise()
{
    p->finalised = true;
}
bool ScriptRegistry::isFinalised() const
{
    return p->finalised;
}

void ScriptRegistry::shutdown()
{
    p->operationState.fetch_or(ScriptRegistryPrivate::ShutdownFlag);

    auto state = p->operationState.load();
    while(state != ScriptRegistryPrivate::ShutdownFlag) {
        p->operationState.wait(state);
        state = p->operationState.load();
    }

    p->variableIds.clear();
    p->functionIds.clear();
    p->variables.clear();
    p->functions.clear();
}

std::span<const ScriptVariableDescriptor> ScriptRegistry::variables() const
{
    return p->variables;
}

std::span<const ScriptFunctionDescriptor> ScriptRegistry::functions() const
{
    return p->functions;
}

VariableKind ScriptRegistry::resolveVariable(const QString& name) const
{
    const auto key = name.toUpper();
    if(const auto it = p->variableIds.find(key); it != p->variableIds.end()) {
        return p->variables[it->second].kind;
    }
    return VariableKind::Generic;
}

bool ScriptRegistry::isVariable(const QString& name) const
{
    const auto key = name.toUpper();
    return p->variableIds.contains(key);
}

ScriptFunctionId ScriptRegistry::resolveFunctionId(const QString& name) const
{
    if(const auto it = p->functionIds.find(name.toLower()); it != p->functionIds.end()) {
        return it->second;
    }
    return InvalidScriptFunctionId;
}

FunctionKind ScriptRegistry::functionKind(ScriptFunctionId id) const
{
    return id != InvalidScriptFunctionId && id <= p->functions.size() ? p->functions[id - 1].kind
                                                                      : FunctionKind::Generic;
}

const ScriptVariableInvoker* ScriptRegistry::variableInvoker(const QString& name) const
{
    if(const auto it = p->variableIds.find(name.toUpper()); it != p->variableIds.end()) {
        const auto& invoker = p->variables[it->second].invoker;
        return invoker ? &invoker : nullptr;
    }
    return nullptr;
}

const ScriptVariableDescriptor* ScriptRegistry::variableDescriptor(const QString& name) const
{
    const auto it = p->variableIds.find(name.toUpper());
    return it != p->variableIds.end() ? &p->variables[it->second] : nullptr;
}

const ScriptFunctionInvoker* ScriptRegistry::functionInvoker(ScriptFunctionId id) const
{
    return id != InvalidScriptFunctionId && id <= p->functions.size() ? &p->functions[id - 1].invoker : nullptr;
}

const ScriptFunctionDescriptor* ScriptRegistry::functionDescriptor(ScriptFunctionId id) const
{
    return id != InvalidScriptFunctionId && id <= p->functions.size() ? &p->functions[id - 1] : nullptr;
}

bool ScriptRegistry::beginOperation()
{
    auto state = p->operationState.load();
    while(!(state & ScriptRegistryPrivate::ShutdownFlag)) {
        if(p->operationState.compare_exchange_weak(state, state + 1)) {
            return true;
        }
    }
    return false;
}

void ScriptRegistry::endOperation()
{
    if(p->operationState.fetch_sub(1) == ScriptRegistryPrivate::ShutdownFlag + 1) {
        p->operationState.notify_all();
    }
}
} // namespace Fooyin
