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

#include "scriptevaluator.h"

#include "functions/controlfuncs.h"
#include "functions/mathfuncs.h"
#include "functions/stringfuncs.h"
#include "functions/timefuncs.h"
#include "functions/tracklistfuncs.h"
#include "scriptbinder.h"
#include "version.h"

#include <core/constants.h>
#include <core/playlist/playlist.h>
#include <core/scripting/scriptdefs.h>
#include <core/track.h>
#include <utils/audioutils.h>
#include <utils/stringutils.h>
#include <utils/utils.h>

#include <QDateTime>
#include <QDir>
#include <QRegularExpression>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
RatingStarSymbols ratingStarSymbols(const ScriptEvaluationOptions& options)
{
    auto symbols = options.ratingSymbols;
    if(symbols.fullStarSymbol.isEmpty()) {
        symbols.fullStarSymbol = defaultRatingFullStarSymbol();
    }
    if(symbols.halfStarSymbol.isEmpty()) {
        symbols.halfStarSymbol = defaultRatingHalfStarSymbol();
    }
    return symbols;
}

uint64_t remainingPlaybackSeconds(uint64_t duration, uint64_t position)
{
    return (duration / 1000) - (position / 1000);
}

QString trackChannels(const Track& track)
{
    switch(track.channels()) {
        case 1:
            return u"Mono"_s;
        case 2:
            return u"Stereo"_s;
        default:
            return u"%1ch"_s.arg(track.channels());
    }
}

QString formatGain(const float gain)
{
    return u"%1 dB"_s.arg(gain);
}

QString formatPeak(const float peak)
{
    const double dbPeak = Audio::volumeToDb(static_cast<double>(peak));
    return u"%1 dB"_s.arg(dbPeak, 0, 'f', 2).prepend(dbPeak > 0 ? "+"_L1 : ""_L1);
}

QString formatDateTime(const uint64_t ms)
{
    if(ms == 0) {
        return {};
    }

    return Utils::msToDateString(static_cast<int64_t>(ms));
}

QString sampleRateMetadata(const Track& track)
{
    return track.sampleRate() > 0 ? QString::number(track.sampleRate()) : QString{};
}

int bitDepthMetadata(const Track& track)
{
    return track.bitDepth() > 0 ? track.bitDepth() : -1;
}

ScriptContext makeContext(const ScriptContext& base, const Track* track, const TrackList* tracks,
                          const Playlist* playlist)
{
    ScriptContext context = base;
    context.track         = track;
    context.tracks        = tracks ? tracks : base.tracks;
    context.playlist      = playlist;
    return context;
}

ScriptResult unavailableTrackListResult(const ScriptEvaluationOptions& options, const QString& var)
{
    const auto policy          = options.trackListContextPolicy;
    const QString& placeholder = options.trackListPlaceholder;

    if(policy == TrackListContextPolicy::Placeholder && !placeholder.isNull()) {
        return {.value = placeholder, .cond = true};
    }

    return {.value = u"%%1%"_s.arg(var), .cond = true};
}

void applyOutputPolicy(const ScriptEvaluationOptions& options, ScriptResult& result)
{
    const bool replacePathSeparators = options.replacePathSeparators;

    result.value = escapeRichText(&options, result.value);

    if(replacePathSeparators && !result.value.isEmpty()) {
        static const QRegularExpression regex{uR"([/\\])"_s};
        result.value.replace(regex, "-"_L1);
    }
}

bool preservesPathSeparators(const VariableKind kind)
{
    switch(kind) {
        case VariableKind::FilePath:
        case VariableKind::Directory:
        case VariableKind::Path:
        case VariableKind::LibraryPath:
        case VariableKind::RelativePath:
            return true;
        default:
            return false;
    }
}

void applyVariableOutputPolicy(const ScriptEvaluationOptions& options, const VariableKind kind, ScriptResult& result)
{
    if(preservesPathSeparators(kind)) {
        result.value = escapeRichText(&options, result.value);
        return;
    }

    applyOutputPolicy(options, result);
}

const TrackList* contextTrackList(const ScriptContext& context)
{
    return context.tracks;
}

const ScriptPlaybackData* playbackData(const ScriptContext& context)
{
    return context.playback;
}

bool isTrackListVariableKind(const VariableKind kind)
{
    switch(kind) {
        case VariableKind::AverageBitrate:
        case VariableKind::Genres:
        case VariableKind::TrackCount:
        case VariableKind::Playtime:
        case VariableKind::PlaylistSize:
        case VariableKind::PlaylistDuration:
        case VariableKind::PlaylistElapsed:
            return true;
        default:
            return false;
    }
}

std::optional<ScriptEvaluator::FuncRet> trackMetadataValue(const VariableKind kind, const Track& track,
                                                           const ScriptEvaluator& registry)
{
    switch(kind) {
        case VariableKind::Track:
            return track.trackNumber();
        case VariableKind::TrackTotal:
            return track.trackTotal();
        case VariableKind::Disc:
            return track.discNumber();
        case VariableKind::DiscTotal:
            return track.discTotal();
        case VariableKind::Title:
            return track.effectiveTitle();
        case VariableKind::Artist:
            return track.primaryArtist();
        case VariableKind::TrackArtist:
            return track.uniqueArtist();
        case VariableKind::PlayCount:
            return track.playCount();
        case VariableKind::Duration: {
            const auto duration = track.duration();
            return duration == 0 ? ScriptEvaluator::FuncRet{QString{}}
                                 : ScriptEvaluator::FuncRet{Utils::msToString(duration)};
        }
        case VariableKind::DurationSecs: {
            const auto duration = track.duration();
            return duration == 0 ? ScriptEvaluator::FuncRet{QString{}}
                                 : ScriptEvaluator::FuncRet{QString::number(duration / 1000)};
        }
        case VariableKind::DurationMSecs: {
            const auto duration = track.duration();
            return duration == 0 ? ScriptEvaluator::FuncRet{QString{}}
                                 : ScriptEvaluator::FuncRet{QString::number(duration)};
        }
        case VariableKind::AlbumArtist:
            return registry.albumArtistMetadata(track);
        case VariableKind::Album:
            return track.album();
        case VariableKind::Genre:
        case VariableKind::Genres:
            return track.genre();
        case VariableKind::Composer:
            return track.composer();
        case VariableKind::Performer:
            return track.performer();
        case VariableKind::Comment:
            return track.comment();
        case VariableKind::Date:
            return track.date();
        case VariableKind::Year:
            return track.year();
        case VariableKind::FileSize:
            return track.fileSize();
        case VariableKind::FileSizeNatural:
            return Utils::formatFileSize(track.fileSize());
        case VariableKind::Bitrate:
            return registry.bitrateMetadata(track);
        case VariableKind::SampleRate:
            return sampleRateMetadata(track);
        case VariableKind::BitDepth:
            return bitDepthMetadata(track);
        case VariableKind::FirstPlayed:
            return formatDateTime(track.firstPlayed());
        case VariableKind::LastPlayed:
            return formatDateTime(track.lastPlayed());
        case VariableKind::Rating:
            return track.rating() > 0 ? track.rating() * 5.0F : -1.0F;
        case VariableKind::RatingNormalized:
            return track.rating();
        case VariableKind::Stars:
            return track.rating() > 0 ? track.rating() * 5.0F : -1.0F;
        case VariableKind::RatingStars:
            return formattedRatingStars(track, registry.options, false);
        case VariableKind::RatingStarsPadded:
            return formattedRatingStars(track, registry.options, true);
        case VariableKind::Loved:
        case VariableKind::LoveEditor:
            return track.isLoved() ? u"1"_s : QString{};
        case VariableKind::RatingEditor:
            return track.ratingStars();
        case VariableKind::Codec:
            return !track.codec().isEmpty() ? track.codec() : track.extension().toUpper();
        case VariableKind::CodecProfile:
            return track.codecProfile();
        case VariableKind::Tool:
            return track.tool();
        case VariableKind::TagType:
            return track.tagType(u" | "_s);
        case VariableKind::Encoding:
            return track.encoding();
        case VariableKind::Channels:
            return trackChannels(track);
        case VariableKind::CreatedTime:
            return formatDateTime(track.createdTime());
        case VariableKind::AddedTime:
            return formatDateTime(track.addedTime());
        case VariableKind::LastModified:
            return formatDateTime(track.lastModified());
        case VariableKind::FilePath:
            return track.filepath();
        case VariableKind::FileName:
            return track.filename();
        case VariableKind::Extension:
            return track.extension();
        case VariableKind::FileNameWithExt:
            return track.filenameExt();
        case VariableKind::Directory:
            return track.directory();
        case VariableKind::Path:
            return track.path();
        case VariableKind::Subsong:
            return track.subsong();
        case VariableKind::RGTrackGain:
            if(track.hasEffectiveTrackGain()) {
                return formatGain(track.effectiveRGTrackGain());
            }
            return QString{};
        case VariableKind::RGTrackPeak:
            if(track.hasEffectiveTrackPeak()) {
                return track.effectiveRGTrackPeak();
            }
            return QString{};
        case VariableKind::RGTrackPeakDB:
            if(track.hasEffectiveTrackPeak()) {
                return formatPeak(track.effectiveRGTrackPeak());
            }
            return QString{};
        case VariableKind::RGAlbumGain:
            if(track.hasEffectiveAlbumGain()) {
                return formatGain(track.effectiveRGAlbumGain());
            }
            return QString{};
        case VariableKind::RGAlbumPeak:
            if(track.hasEffectiveAlbumPeak()) {
                return track.effectiveRGAlbumPeak();
            }
            return QString{};
        case VariableKind::RGAlbumPeakDB:
            if(track.hasEffectiveAlbumPeak()) {
                return formatPeak(track.effectiveRGAlbumPeak());
            }
            return QString{};
        default:
            return {};
    }
}

std::optional<ScriptEvaluator::FuncRet> trackListValue(const VariableKind kind, const TrackList& tracks,
                                                       const ScriptEvaluator& registry)
{
    const auto& cache = registry.cachedTrackListValues(tracks);

    switch(kind) {
        case VariableKind::AverageBitrate:
            return cache.averageBitrate;
        case VariableKind::TrackCount:
            return cache.trackCount;
        case VariableKind::Playtime:
        case VariableKind::PlaylistDuration:
            return cache.playtime;
        case VariableKind::PlaylistSize:
            return cache.playlistSize;
        case VariableKind::PlaylistElapsed:
            return cache.playlistElapsed;
        case VariableKind::Genres:
            return cache.genres;
        default:
            return {};
    }
}

std::optional<QString> playbackVariableValue(const VariableKind kind, const ScriptEvaluator& registry,
                                             const Track& track)
{
    switch(kind) {
        case VariableKind::PlaybackTime:
            return registry.playbackValueAvailableForTrack(track) ? std::optional{registry.playbackTime()}
                                                                  : std::optional<QString>{};
        case VariableKind::PlaybackTimeSeconds:
            return registry.playbackValueAvailableForTrack(track) ? std::optional{registry.playbackTimeSeconds()}
                                                                  : std::optional<QString>{};
        case VariableKind::PlaybackTimeRemaining:
            return registry.playbackValueAvailableForTrack(track) ? std::optional{registry.playbackTimeRemaining()}
                                                                  : std::optional<QString>{};
        case VariableKind::PlaybackTimeRemainingSeconds:
            return registry.playbackValueAvailableForTrack(track)
                     ? std::optional{registry.playbackTimeRemainingSeconds()}
                     : std::optional<QString>{};
        case VariableKind::InputDecoder:
            return registry.playbackValueAvailableForTrack(track) ? std::optional{registry.decoder()}
                                                                  : std::optional<QString>{};
        case VariableKind::OutputSampleRate:
        case VariableKind::OutputChannels:
        case VariableKind::OutputChannelMask:
        case VariableKind::OutputBitDepth:
        case VariableKind::OutputDevice:
        case VariableKind::OutputDsps:
        case VariableKind::OutputDspPreset:
        case VariableKind::OutputVolume:
        case VariableKind::OutputRgSource:
        case VariableKind::OutputRgMode:
        case VariableKind::OutputRgGain:
        case VariableKind::OutputRgPeak:
        case VariableKind::OutputRgPeakDb:
        case VariableKind::OutputBufferLength:
            return registry.playbackValueAvailableForTrack(track) ? std::optional{registry.outputInfo(kind)}
                                                                  : std::optional<QString>{};
        case VariableKind::IsPlaying:
            return registry.playbackValueAvailableForTrack(track) ? std::optional{registry.isPlaying()}
                                                                  : std::optional<QString>{};
        case VariableKind::IsPaused:
            return registry.playbackValueAvailableForTrack(track) ? std::optional{registry.isPaused()}
                                                                  : std::optional<QString>{};
        case VariableKind::IsStopped:
            return registry.isStopped();
        default:
            return {};
    }
}

std::optional<QString> libraryVariableValue(const VariableKind kind, const Track& track,
                                            const ScriptEvaluator& registry)
{
    switch(kind) {
        case VariableKind::LibraryName:
            return registry.libraryNameVariable(track);
        case VariableKind::LibraryPath:
            return registry.libraryPathVariable(track);
        case VariableKind::RelativePath:
            return registry.relativePathVariable(track);
        default:
            return {};
    }
}
} // namespace

QString escapeRichText(const ScriptEvaluationOptions* options, QString value)
{
    if(options && options->escapeRichText) {
        value.replace(u'\\', u"\\\\"_s);
        value.replace(u'<', u"\\<"_s);
    }
    return value;
}

QString formattedRatingStars(const Track& track, const ScriptEvaluationOptions& options, bool includeEmptyStars)
{
    const int rating = track.ratingStars();

    const RatingStarSymbols symbols = ratingStarSymbols(options);
    const QString emptyStarSymbol   = includeEmptyStars ? symbols.emptyStarSymbol : QString{};

    if(rating <= 0) {
        return includeEmptyStars && !emptyStarSymbol.isEmpty() ? emptyStarSymbol.repeated(5) : QString{};
    }

    const int emptyStars = 5 - (rating / 2) - ((rating % 2) != 0 ? 1 : 0);

    QString text;
    for(int i{0}; i < rating / 2; ++i) {
        text.append(symbols.fullStarSymbol);
    }
    if((rating % 2) != 0) {
        text.append(symbols.halfStarSymbol);
    }
    if(includeEmptyStars && !emptyStarSymbol.isEmpty()) {
        for(int i{0}; i < emptyStars; ++i) {
            text.append(emptyStarSymbol);
        }
    }

    return text;
}

ScriptEvaluator::ScriptEvaluator(ScriptRegistry* registry)
    : m_registry{registry}
{ }

ScriptEvaluator::~ScriptEvaluator() = default;

ScriptResult ScriptEvaluator::value(VariableKind kind, const QString& var, const ScriptSubject& subject) const
{
    const auto* playlist   = subject.playlist ? subject.playlist : currentContext().playlist;
    const auto* descriptor = m_registry->variableDescriptor(var);
    if(descriptor && descriptor->contextual) {
        for(const auto& value : currentContext().values) {
            if(value.name.compare(descriptor->name, Qt::CaseInsensitive) == 0) {
                if(value.track.isValid() && (!subject.track || !subject.track->sameIdentityAs(value.track))) {
                    return {};
                }
                if(descriptor->playbackScoped && !playbackValueAvailableForTrack(value.track)) {
                    return {};
                }
                auto result{value.result};
                applyVariableOutputPolicy(options, kind, result);
                return result;
            }
        }
        return {};
    }

    if(const auto* invoker = m_registry->variableInvoker(var)) {
        const auto context = makeContext(currentContext(), subject.track, subject.tracks, playlist);
        auto result        = (*invoker)(context, var);
        applyVariableOutputPolicy(options, kind, result);
        return result;
    }
    for(const auto& value : currentContext().values) {
        if(value.name.compare(var, Qt::CaseInsensitive) == 0) {
            auto result = value.result;
            applyVariableOutputPolicy(options, kind, result);
            return result;
        }
    }
    if(kind == VariableKind::PlaylistName) {
        QString name;
        if(playlist) {
            name = playlist->name();
        }
        else if(const auto* row = currentContext().playlistData) {
            name = row->name;
        }
        ScriptResult result = calculateResult(name);
        applyVariableOutputPolicy(options, kind, result);
        return result;
    }

    if(subject.track) {
        return valueForTrack(kind, var, *subject.track, playlist);
    }

    if(subject.tracks) {
        return valueForTrackList(kind, var, *subject.tracks);
    }

    if(subject.playlist) {
        return valueForPlaylist(kind, var, *subject.playlist);
    }

    return {};
}

ScriptResult ScriptEvaluator::function(ScriptFunctionId functionId, const QString& func, const ScriptValueList& args,
                                       const ScriptSubject& subject) const
{
    const auto* invoker = m_registry->functionInvoker(functionId);
    if(!invoker && !func.isEmpty()) {
        functionId = m_registry->resolveFunctionId(func);
        invoker    = m_registry->functionInvoker(functionId);
    }
    if(!invoker || !(*invoker)) {
        return {};
    }

    Track playlistTrack;
    const Track* track       = subject.track;
    const TrackList* tracks  = subject.tracks ? subject.tracks : currentContext().tracks;
    const Playlist* playlist = subject.playlist ? subject.playlist : currentContext().playlist;

    if(track == nullptr && subject.tracks != nullptr) {
        if(!subject.tracks->empty()) {
            track = &subject.tracks->front();
        }
        tracks = subject.tracks;
    }
    else if(track == nullptr && subject.playlist != nullptr) {
        playlistTrack = subject.playlist->currentTrack();
        if(playlistTrack.isValid()) {
            track = &playlistTrack;
        }

        playlist = subject.playlist;
    }

    const auto context = makeContext(currentContext(), track, tracks, playlist);
    const ScriptFunctionCallContext call{
        .context = &context,
        .options = &options,
        .args    = args,
        .subject = {.track = track, .tracks = tracks, .playlist = playlist},
    };
    return (*invoker)(call);
}

const ScriptContext& ScriptEvaluator::currentContext() const
{
    return *m_context;
}

ScriptResult ScriptEvaluator::valueForTrack(VariableKind kind, const QString& var, const Track& track,
                                            const Playlist* playlist) const
{
    if(isTrackListVariableKind(kind)) {
        if(options.trackListContextPolicy == TrackListContextPolicy::Fallback) {
            if(const auto* tracks = contextTrackList(currentContext()); tracks != nullptr) {
                return value(kind, var, makeScriptSubject(*tracks));
            }
            if(playlist) {
                return value(kind, var, makeScriptSubject(playlist->tracks()));
            }
        }
        return unavailableTrackListResult(options, var);
    }

    if(const auto value = trackMetadataValue(kind, track, *this); value.has_value()) {
        ScriptResult result = calculateResult(*value);
        applyVariableOutputPolicy(options, kind, result);
        return result;
    }
    if(const auto value = playbackVariableValue(kind, *this, track); value.has_value()) {
        ScriptResult result = calculateResult(*value);
        applyVariableOutputPolicy(options, kind, result);
        return result;
    }
    if(const auto value = libraryVariableValue(kind, track, *this); value.has_value()) {
        ScriptResult result = calculateResult(*value);
        applyVariableOutputPolicy(options, kind, result);
        return result;
    }

    if(m_registry->isVariable(var)) {
        return {};
    }
    ScriptResult result = calculateResult(track.extraTag(var));
    applyOutputPolicy(options, result);
    return result;
}

ScriptResult ScriptEvaluator::valueForTrackList(VariableKind kind, const QString& var, const TrackList& tracks) const
{
    if(const auto value = trackListValue(kind, tracks, *this); value.has_value()) {
        ScriptResult result = calculateResult(*value);
        applyVariableOutputPolicy(options, kind, result);
        return result;
    }

    if(tracks.empty()) {
        return {};
    }

    return value(kind, var, makeScriptSubject(tracks.front()));
}

ScriptResult ScriptEvaluator::valueForPlaylist(VariableKind kind, const QString& var, const Playlist& playlist) const
{
    if(var.isEmpty()) {
        return {};
    }

    if(isTrackListVariableKind(kind)) {
        switch(kind) {
            case VariableKind::AverageBitrate:
            case VariableKind::TrackCount:
            case VariableKind::Playtime:
            case VariableKind::PlaylistSize:
            case VariableKind::PlaylistDuration:
            case VariableKind::PlaylistElapsed:
            case VariableKind::Genres:
                return value(kind, var, makeScriptSubject(playlist.tracks()));
            default:
                break;
        }
    }

    if(auto track = playlist.track(playlist.currentTrackIndex())) {
        return value(kind, var, makeScriptSubject(track.value()));
    }

    return {};
}

ScriptResult ScriptEvaluator::calculateResult(const FuncRet& funcRet) const
{
    ScriptResult result;

    if(const auto* intVal = std::get_if<int>(&funcRet)) {
        result.value = QString::number(*intVal);
        result.cond  = (*intVal) >= 0;
    }
    else if(const auto* uintVal = std::get_if<uint64_t>(&funcRet)) {
        result.value = QString::number(*uintVal);
        result.cond  = true;
    }
    else if(const auto* floatVal = std::get_if<float>(&funcRet)) {
        result.value = QString::number(*floatVal);
        result.cond  = (*floatVal) >= 0;
    }
    else if(const auto* strVal = std::get_if<QString>(&funcRet)) {
        result.value = *strVal;
        result.cond  = !result.value.isEmpty();
    }
    else if(const auto* strListVal = std::get_if<QStringList>(&funcRet)) {
        result.value = strListVal->empty() ? QString{} : strListVal->join(QLatin1String{Constants::UnitSeparator});
        result.cond  = !result.value.isEmpty();
    }

    return result;
}

ScriptEvaluator::FuncRet ScriptEvaluator::albumArtistMetadata(const Track& track) const
{
    return track.effectiveAlbumArtist(options.useVariousArtists);
}

ScriptEvaluator::FuncRet ScriptEvaluator::bitrateMetadata(const Track& track) const
{
    return getBitrate(track);
}

bool ScriptEvaluator::playbackValueAvailableForTrack(const Track& track) const
{
    const auto* playback = playbackData(currentContext());
    if(playback && playback->playState == Player::PlayState::Stopped) {
        return false;
    }

    const auto* playlistEnvironment = currentContext().playlistData;
    if(!playlistEnvironment) {
        return true;
    }

    const int currentTrackIndex        = playlistEnvironment->trackIndex;
    const int currentPlayingTrackIndex = playlistEnvironment->playingTrackIndex;
    const int currentPlayingTrackId    = playlistEnvironment->playingTrackId;

    if(playlistEnvironment->queueItemState != ScriptQueueItemState::None) {
        return playlistEnvironment->queueItemState == ScriptQueueItemState::Current
            && (!track.isValid() || currentPlayingTrackId < 0 || track.id() == currentPlayingTrackId);
    }

    if(currentTrackIndex < 0) {
        return !track.isValid() || currentPlayingTrackId < 0 || track.id() == currentPlayingTrackId;
    }

    const bool indexMatches = currentPlayingTrackIndex >= 0 && currentTrackIndex == currentPlayingTrackIndex;
    const bool trackMatches = !track.isValid() || currentPlayingTrackId < 0 || track.id() == currentPlayingTrackId;

    return indexMatches && trackMatches;
}

QString ScriptEvaluator::playbackTime() const
{
    if(!playbackValueAvailableForTrack()) {
        return {};
    }
    if(const auto* playback = playbackData(currentContext()); playback != nullptr) {
        return Utils::msToString(playback->position);
    }
    return {};
}

QString ScriptEvaluator::playbackTimeSeconds() const
{
    if(!playbackValueAvailableForTrack()) {
        return {};
    }
    if(const auto* playback = playbackData(currentContext()); playback != nullptr) {
        return QString::number(playback->position / 1000);
    }
    return {};
}
QString ScriptEvaluator::playbackTimeRemaining() const
{
    if(!playbackValueAvailableForTrack()) {
        return {};
    }
    if(const auto* playback = playbackData(currentContext()); playback != nullptr) {
        if(playback->duration == 0 || playback->position >= playback->duration) {
            return {};
        }
        const auto remaining = remainingPlaybackSeconds(playback->duration, playback->position);
        return Utils::msToString(remaining * 1000);
    }
    return {};
}

QString ScriptEvaluator::playbackTimeRemainingSeconds() const
{
    if(!playbackValueAvailableForTrack()) {
        return {};
    }
    if(const auto* playback = playbackData(currentContext()); playback != nullptr) {
        if(playback->duration == 0 || playback->position >= playback->duration) {
            return {};
        }
        return QString::number(remainingPlaybackSeconds(playback->duration, playback->position));
    }
    return {};
}

QString ScriptEvaluator::decoder() const
{
    if(!playbackValueAvailableForTrack()) {
        return {};
    }
    if(const auto* playback = playbackData(currentContext()); playback != nullptr) {
        return playback->decoder;
    }
    return {};
}

QString ScriptEvaluator::outputInfo(VariableKind kind) const
{
    const auto* playback = playbackData(currentContext());
    if(!playback) {
        return {};
    }

    const Engine::PlaybackOutputInfo& info = playback->outputInfo;
    const auto formatDb                    = [](double value) {
        return QString::number(value, 'f', 2);
    };

    switch(kind) {
        case VariableKind::OutputSampleRate:
            return info.format.isValid() ? QString::number(info.format.sampleRate()) : QString{};
        case VariableKind::OutputChannels:
            return info.format.isValid() ? QString::number(info.format.channelCount()) : QString{};
        case VariableKind::OutputChannelMask:
            return info.format.isValid() && info.format.hasChannelLayout() ? info.format.prettyChannelLayout()
                                                                           : QString{};
        case VariableKind::OutputBitDepth:
            return info.format.isValid() ? QString::number(info.format.bitdepth()) : QString{};
        case VariableKind::OutputDevice:
            return info.device;
        case VariableKind::OutputDsps: {
            QStringList dsps;
            dsps.reserve(static_cast<qsizetype>(info.dsps.size()));
            std::ranges::copy(info.dsps, std::back_inserter(dsps));
            return dsps.join(u", "_s);
        }
        case VariableKind::OutputDspPreset:
            return info.dspPreset;
        case VariableKind::OutputVolume:
            return info.volume > 0.0 ? formatDb(Audio::volumeToDb(info.volume)) + u" dB"_s : u"-inf"_s;
        case VariableKind::OutputRgSource:
            switch(info.replayGain.source) {
                case Engine::ReplayGainSource::Track:
                    return tr("Track");
                case Engine::ReplayGainSource::Album:
                    return tr("Album");
                case Engine::ReplayGainSource::None:
                default:
                    return tr("None");
            }
        case VariableKind::OutputRgMode: {
            const bool gain = info.replayGain.processing.testFlag(Engine::ApplyGain);
            const bool clip = info.replayGain.processing.testFlag(Engine::PreventClipping);
            if(gain && clip) {
                return tr("Gain and clip prevention");
            }
            if(gain) {
                return tr("Gain");
            }
            if(clip) {
                return tr("Clip prevention");
            }
            return tr("None");
        }
        case VariableKind::OutputRgGain:
            return formatDb(info.replayGain.gainDb) + u" dB"_s;
        case VariableKind::OutputRgPeak:
            return info.replayGain.hasPeak ? QString::number(info.replayGain.peak, 'g', 10) : QString{};
        case VariableKind::OutputRgPeakDb:
            return info.replayGain.hasPeak && info.replayGain.peak > 0.0
                     ? formatDb(Audio::volumeToDb(info.replayGain.peak)) + u" dBFS"_s
                     : QString{};
        case VariableKind::OutputBufferLength:
            return info.bufferLengthMs > 0 ? QString::number(info.bufferLengthMs) : QString{};
        default:
            return {};
    }
}

QString ScriptEvaluator::isPlaying() const
{
    if(!playbackValueAvailableForTrack()) {
        return {};
    }
    if(const auto* playback = playbackData(currentContext());
       playback != nullptr && playback->playState == Player::PlayState::Playing) {
        return u"1"_s;
    }
    return {};
}

QString ScriptEvaluator::isPaused() const
{
    if(!playbackValueAvailableForTrack()) {
        return {};
    }
    if(const auto* playback = playbackData(currentContext());
       playback != nullptr && playback->playState == Player::PlayState::Paused) {
        return u"1"_s;
    }
    return {};
}

QString ScriptEvaluator::isStopped() const
{
    if(const auto* playback = playbackData(currentContext());
       playback != nullptr && playback->playState == Player::PlayState::Stopped) {
        return u"1"_s;
    }
    return {};
}

QString ScriptEvaluator::libraryNameVariable(const Track& track) const
{
    if(currentContext().libraries) {
        if(const auto it = currentContext().libraries->find(track.libraryId());
           it != currentContext().libraries->end()) {
            return it->second.name;
        }
    }
    return {};
}

QString ScriptEvaluator::libraryPathVariable(const Track& track) const
{
    if(currentContext().libraries) {
        if(const auto it = currentContext().libraries->find(track.libraryId());
           it != currentContext().libraries->end()) {
            return it->second.path;
        }
    }
    return {};
}

QString ScriptEvaluator::relativePathVariable(const Track& track) const
{
    const QString path = libraryPathVariable(track);
    return path.isEmpty() ? QString{} : QDir{path}.relativeFilePath(track.prettyFilepath());
}

QString ScriptEvaluator::getBitrate(const Track& track) const
{
    int bitrate = track.bitrate();
    if(const auto* playback = playbackData(currentContext());
       playback != nullptr && playback->bitrate > 0 && playbackValueAvailableForTrack(track)) {
        bitrate = playback->bitrate;
    }
    return bitrate > 0 ? QString::number(bitrate) : QString{};
}

QString ScriptEvaluator::playlistDuration(const TrackList& tracks) const
{
    const auto* playback = playbackData(currentContext());
    if(!playback || tracks.empty()) {
        return {};
    }

    const int currentTrackIndex = currentContext().playlistData ? currentContext().playlistData->trackIndex : -1;
    if(currentTrackIndex < 0 || std::cmp_greater_equal(currentTrackIndex, tracks.size())) {
        return {};
    }
    uint64_t total{0};

    for(auto i{0}; i <= currentTrackIndex; ++i) {
        total += tracks[i].duration();
    }

    total -= playback->duration;
    total += playback->position;

    return Utils::msToString(total);
}

void ScriptEvaluator::clearTrackListCache()
{
    m_trackListCache = {};
}

const ScriptEvaluator::TrackListAggregateCache& ScriptEvaluator::cachedTrackListValues(const TrackList& tracks) const
{
    const int trackCount      = static_cast<int>(tracks.size());
    const auto* playback      = playbackData(currentContext());
    const int currentPosition = playback ? static_cast<int>(playback->position) : -1;
    const int playlistIndex   = currentContext().playlistData ? currentContext().playlistData->trackIndex : -1;

    if(m_trackListCache.tracks != &tracks || m_trackListCache.trackCount != trackCount
       || m_trackListCache.currentPosition != currentPosition || m_trackListCache.playlistIndex != playlistIndex) {
        m_trackListCache.tracks          = &tracks;
        m_trackListCache.trackCount      = trackCount;
        m_trackListCache.currentPosition = currentPosition;
        m_trackListCache.playlistIndex   = playlistIndex;
        m_trackListCache.playtime        = Scripting::playtime(tracks);
        m_trackListCache.averageBitrate  = Scripting::averageBitrate(tracks);
        m_trackListCache.playlistSize    = Scripting::playlistSize(tracks);
        m_trackListCache.genres          = Scripting::genres(tracks);
        m_trackListCache.playlistElapsed = playlistDuration(tracks);
    }

    return m_trackListCache;
}
} // namespace Fooyin
