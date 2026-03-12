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

#include "scriptregistry.h"

#include "functions/controlfuncs.h"
#include "functions/mathfuncs.h"
#include "functions/stringfuncs.h"
#include "functions/timefuncs.h"
#include "functions/tracklistfuncs.h"
#include "scriptbinder.h"

#include <core/constants.h>
#include <core/playlist/playlist.h>
#include <core/scripting/scriptproviders.h>
#include <core/track.h>
#include <utils/audioutils.h>
#include <utils/stringutils.h>
#include <utils/utils.h>

#include <QDir>
#include <QRegularExpression>

using namespace Qt::StringLiterals;

namespace {
QString trackMeta(const Fooyin::Track& track, const QStringList& args)
{
    if(args.empty()) {
        return {};
    }

    return track.metaValue(args.front());
}

QString trackInfo(const Fooyin::Track& track, const QStringList& args)
{
    if(args.empty()) {
        return {};
    }

    return track.techInfo(args.front());
}

QString trackChannels(const Fooyin::Track& track)
{
    switch(track.channels()) {
        case(1):
            return u"Mono"_s;
        case(2):
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
    const double dbPeak = Fooyin::Audio::volumeToDb(static_cast<double>(peak));
    return u"%1 dB"_s.arg(dbPeak, 0, 'f', 2).prepend(dbPeak > 0 ? "+"_L1 : ""_L1);
}

QString formatDateTime(const uint64_t ms)
{
    if(ms == 0) {
        return {};
    }

    return Fooyin::Utils::msToDateString(static_cast<int64_t>(ms));
}

QString sampleRateMetadata(const Fooyin::Track& track)
{
    return track.sampleRate() > 0 ? QString::number(track.sampleRate()) : QString{};
}

int bitDepthMetadata(const Fooyin::Track& track)
{
    return track.bitDepth() > 0 ? track.bitDepth() : -1;
}

Fooyin::ScriptContext makeContext(const Fooyin::ScriptContext& base, const Fooyin::Track* track,
                                  const Fooyin::TrackList* tracks, const Fooyin::Playlist* playlist)
{
    Fooyin::ScriptContext context = base;
    context.track                 = track;
    context.tracks                = tracks;
    context.playlist              = playlist;
    return context;
}

Fooyin::ScriptResult unavailableTrackListResult(const Fooyin::ScriptContext& context, const QString& var)
{
    const auto* environment = context.environment ? context.environment->evaluationEnvironment() : nullptr;
    const Fooyin::TrackListContextPolicy policy
        = environment ? environment->trackListContextPolicy() : Fooyin::TrackListContextPolicy::Unresolved;
    const QString placeholder = environment ? environment->trackListPlaceholder() : QString{};

    if(policy == Fooyin::TrackListContextPolicy::Placeholder && !placeholder.isNull()) {
        return {.value = placeholder, .cond = true};
    }

    return {.value = u"%%1%"_s.arg(var), .cond = true};
}

void applyOutputPolicy(const Fooyin::ScriptContext& context, Fooyin::ScriptResult& result)
{
    const auto* environment          = context.environment ? context.environment->evaluationEnvironment() : nullptr;
    const bool escapeRichText        = environment && environment->escapeRichText();
    const bool replacePathSeparators = environment && environment->replacePathSeparators();

    if(escapeRichText && !result.value.isEmpty()) {
        result.value.replace(u'<', u"\\<"_s);
    }

    if(replacePathSeparators && !result.value.isEmpty()) {
        static const QRegularExpression regex{uR"([/\\])"_s};
        result.value.replace(regex, "-"_L1);
    }
}

Fooyin::TrackListContextPolicy trackListContextPolicy(const Fooyin::ScriptContext& context)
{
    if(const auto* environment = context.environment ? context.environment->evaluationEnvironment() : nullptr) {
        return environment->trackListContextPolicy();
    }

    return Fooyin::TrackListContextPolicy::Unresolved;
}

bool useVariousArtists(const Fooyin::ScriptContext& context)
{
    if(const auto* environment = context.environment ? context.environment->evaluationEnvironment() : nullptr) {
        return environment->useVariousArtists();
    }

    return false;
}

const Fooyin::TrackList* contextTrackList(const Fooyin::ScriptContext& context)
{
    if(const auto* environment = context.environment ? context.environment->trackListEnvironment() : nullptr) {
        return environment->trackList();
    }

    return context.tracks;
}

const Fooyin::ScriptPlaybackEnvironment* playbackEnvironment(const Fooyin::ScriptContext& context)
{
    return context.environment ? context.environment->playbackEnvironment() : nullptr;
}

const Fooyin::ScriptLibraryEnvironment* libraryEnvironment(const Fooyin::ScriptContext& context)
{
    return context.environment ? context.environment->libraryEnvironment() : nullptr;
}

} // namespace

namespace Fooyin {
bool isTrackListVariableKind(const VariableKind kind)
{
    switch(kind) {
        case VariableKind::Genres:
        case VariableKind::TrackCount:
        case VariableKind::Playtime:
        case VariableKind::PlaylistDuration:
        case VariableKind::PlaylistElapsed:
            return true;
        default:
            return false;
    }

    return false;
}

std::optional<ScriptRegistry::FuncRet> trackMetadataValue(const VariableKind kind, const Track& track,
                                                          const ScriptRegistry& registry)
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
        case VariableKind::UniqueArtist:
            return track.uniqueArtist();
        case VariableKind::PlayCount:
            return track.playCount();
        case VariableKind::Duration: {
            const auto duration = track.duration();
            return duration == 0 ? ScriptRegistry::FuncRet{QString{}}
                                 : ScriptRegistry::FuncRet{Utils::msToString(duration)};
        }
        case VariableKind::DurationSecs: {
            const auto duration = track.duration();
            return duration == 0 ? ScriptRegistry::FuncRet{QString{}}
                                 : ScriptRegistry::FuncRet{QString::number(duration / 1000)};
        }
        case VariableKind::DurationMSecs: {
            const auto duration = track.duration();
            return duration == 0 ? ScriptRegistry::FuncRet{QString{}}
                                 : ScriptRegistry::FuncRet{QString::number(duration)};
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
            return track.rating();
        case VariableKind::RatingStars:
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
            return formatGain(track.rgTrackGain());
        case VariableKind::RGTrackPeak:
            return track.rgTrackPeak();
        case VariableKind::RGTrackPeakDB:
            return formatPeak(track.rgTrackPeak());
        case VariableKind::RGAlbumGain:
            return formatGain(track.rgAlbumGain());
        case VariableKind::RGAlbumPeak:
            return track.rgAlbumPeak();
        case VariableKind::RGAlbumPeakDB:
            return formatPeak(track.rgAlbumPeak());
        default:
            return {};
    }

    return {};
}

std::optional<ScriptRegistry::FuncRet> trackListValue(const VariableKind kind, const TrackList& tracks,
                                                      const ScriptRegistry& registry)
{
    const auto& cache = registry.cachedTrackListValues(tracks);

    switch(kind) {
        case VariableKind::TrackCount:
            return cache.trackCount;
        case VariableKind::Playtime:
        case VariableKind::PlaylistDuration:
            return cache.playtime;
        case VariableKind::PlaylistElapsed:
            return cache.playlistElapsed;
        case VariableKind::Genres:
            return cache.genres;
        default:
            return {};
    }

    return {};
}

std::optional<QString> playbackVariableValue(const VariableKind kind, const ScriptRegistry& registry)
{
    switch(kind) {
        case VariableKind::PlaybackTime:
            return registry.playbackTime();
        case VariableKind::PlaybackTimeSeconds:
            return registry.playbackTimeSeconds();
        case VariableKind::PlaybackTimeRemaining:
            return registry.playbackTimeRemaining();
        case VariableKind::PlaybackTimeRemainingSeconds:
            return registry.playbackTimeRemainingSeconds();
        case VariableKind::IsPlaying:
            return registry.isPlaying();
        case VariableKind::IsPaused:
            return registry.isPaused();
        default:
            return {};
    }

    return {};
}

std::optional<QString> libraryVariableValue(const VariableKind kind, const Track& track, const ScriptRegistry& registry)
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

    return {};
}

ScriptRegistry::FuncRet ScriptRegistry::albumArtistMetadata(const Track& track) const
{
    return track.effectiveAlbumArtist(useVariousArtists(m_context));
}

ScriptRegistry::FuncRet ScriptRegistry::bitrateMetadata(const Track& track) const
{
    return getBitrate(track);
}

QString ScriptRegistry::playbackTime() const
{
    if(const auto* environment = playbackEnvironment(m_context); environment != nullptr) {
        return Utils::msToString(environment->currentPosition());
    }
    return {};
}

QString ScriptRegistry::playbackTimeSeconds() const
{
    if(const auto* environment = playbackEnvironment(m_context); environment != nullptr) {
        return QString::number(environment->currentPosition() / 1000);
    }
    return {};
}

QString ScriptRegistry::playbackTimeRemaining() const
{
    if(const auto* environment = playbackEnvironment(m_context); environment != nullptr) {
        return Utils::msToString(environment->currentTrackDuration() - environment->currentPosition());
    }
    return {};
}

QString ScriptRegistry::playbackTimeRemainingSeconds() const
{
    if(const auto* environment = playbackEnvironment(m_context); environment != nullptr) {
        return QString::number((environment->currentTrackDuration() - environment->currentPosition()) / 1000);
    }
    return {};
}

QString ScriptRegistry::isPlaying() const
{
    if(const auto* environment = playbackEnvironment(m_context);
       environment != nullptr && environment->playState() == Player::PlayState::Playing) {
        return u"1"_s;
    }
    return {};
}

QString ScriptRegistry::isPaused() const
{
    if(const auto* environment = playbackEnvironment(m_context);
       environment != nullptr && environment->playState() == Player::PlayState::Paused) {
        return u"1"_s;
    }
    return {};
}

QString ScriptRegistry::libraryNameVariable(const Track& track) const
{
    if(const auto* environment = libraryEnvironment(m_context); environment != nullptr) {
        return environment->libraryName(track);
    }
    return {};
}

QString ScriptRegistry::libraryPathVariable(const Track& track) const
{
    if(const auto* environment = libraryEnvironment(m_context); environment != nullptr) {
        return environment->libraryPath(track);
    }
    return {};
}

QString ScriptRegistry::relativePathVariable(const Track& track) const
{
    if(const auto* environment = libraryEnvironment(m_context); environment != nullptr) {
        return environment->relativePath(track);
    }
    return {};
}

void ScriptRegistry::addDefaultFunctions()
{
    registerFunction(u"add"_s, makeScriptFunctionInvoker<Scripting::add>());
    registerFunction(u"sub"_s, makeScriptFunctionInvoker<Scripting::sub>());
    registerFunction(u"mul"_s, makeScriptFunctionInvoker<Scripting::mul>());
    registerFunction(u"div"_s, makeScriptFunctionInvoker<Scripting::div>());
    registerFunction(u"min"_s, makeScriptFunctionInvoker<Scripting::min>());
    registerFunction(u"max"_s, makeScriptFunctionInvoker<Scripting::max>());
    registerFunction(u"mod"_s, makeScriptFunctionInvoker<Scripting::mod>());
    registerFunction(u"rand"_s, makeScriptFunctionInvoker<Scripting::rand>());
    registerFunction(u"round"_s, makeScriptFunctionInvoker<Scripting::round>());

    registerFunction(u"num"_s, makeScriptFunctionInvoker<Scripting::num>());
    registerFunction(u"replace"_s, makeScriptFunctionInvoker<Scripting::replace>());
    registerFunction(u"ascii"_s, makeScriptFunctionInvoker<Scripting::ascii>());
    registerFunction(u"slice"_s, makeScriptFunctionInvoker<Scripting::slice>());
    registerFunction(u"chop"_s, makeScriptFunctionInvoker<Scripting::chop>());
    registerFunction(u"left"_s, makeScriptFunctionInvoker<Scripting::left>());
    registerFunction(u"right"_s, makeScriptFunctionInvoker<Scripting::right>());
    registerFunction(u"insert"_s, makeScriptFunctionInvoker<Scripting::insert>());
    registerFunction(u"substr"_s, makeScriptFunctionInvoker<Scripting::substr>());
    registerFunction(u"strstr"_s, makeScriptFunctionInvoker<Scripting::strstr>());
    registerFunction(u"stristr"_s, makeScriptFunctionInvoker<Scripting::stristr>());
    registerFunction(u"strstrlast"_s, makeScriptFunctionInvoker<Scripting::strstrLast>());
    registerFunction(u"stristrlast"_s, makeScriptFunctionInvoker<Scripting::stristrLast>());
    registerFunction(u"split"_s, makeScriptFunctionInvoker<Scripting::split>());
    registerFunction(u"len"_s, makeScriptFunctionInvoker<Scripting::len>());
    registerFunction(u"longest"_s, makeScriptFunctionInvoker<Scripting::longest>());
    registerFunction(u"strcmp"_s, makeScriptFunctionInvoker<Scripting::strcmp>());
    registerFunction(u"stricmp"_s, makeScriptFunctionInvoker<Scripting::stricmp>());
    registerFunction(u"longer"_s, makeScriptFunctionInvoker<Scripting::longer>());
    registerFunction(u"sep"_s, makeScriptFunctionInvoker<Scripting::sep>());
    registerFunction(u"crlf"_s, makeScriptFunctionInvoker<Scripting::crlf>());
    registerFunction(u"tab"_s, makeScriptFunctionInvoker<Scripting::tab>());
    registerFunction(u"swapprefix"_s, makeScriptFunctionInvoker<Scripting::swapPrefix>());
    registerFunction(u"stripprefix"_s, makeScriptFunctionInvoker<Scripting::stripPrefix>());
    registerFunction(u"pad"_s, makeScriptFunctionInvoker<Scripting::pad>());
    registerFunction(u"padright"_s, makeScriptFunctionInvoker<Scripting::padRight>());
    registerFunction(u"repeat"_s, makeScriptFunctionInvoker<Scripting::repeat>());
    registerFunction(u"trim"_s, makeScriptFunctionInvoker<Scripting::trim>());
    registerFunction(u"lower"_s, makeScriptFunctionInvoker<Scripting::lower>());
    registerFunction(u"upper"_s, makeScriptFunctionInvoker<Scripting::upper>());
    registerFunction(u"abbr"_s, makeScriptFunctionInvoker<Scripting::abbr>());
    registerFunction(u"caps"_s, makeScriptFunctionInvoker<Scripting::caps>());
    registerFunction(u"directory"_s, makeScriptFunctionInvoker<Scripting::directory>());
    registerFunction(u"directory_path"_s, makeScriptFunctionInvoker<Scripting::directoryPath>());
    registerFunction(u"elide_end"_s, makeScriptFunctionInvoker<Scripting::elideEnd>());
    registerFunction(u"elide_mid"_s, makeScriptFunctionInvoker<Scripting::elideMid>());
    registerFunction(u"ext"_s, makeScriptFunctionInvoker<Scripting::ext>());
    registerFunction(u"filename"_s, makeScriptFunctionInvoker<Scripting::filename>());
    registerFunction(u"progress"_s, makeScriptFunctionInvoker<Scripting::progress>());
    registerFunction(u"progress2"_s, makeScriptFunctionInvoker<Scripting::progress2>());
    registerFunction(u"doclink"_s, makeScriptFunctionInvoker<Scripting::doclink>());
    registerFunction(u"cmdlink"_s, makeScriptFunctionInvoker<Scripting::cmdlink>());
    registerFunction(u"urlencode"_s, makeScriptFunctionInvoker<Scripting::urlencode>());

    registerFunction(u"timems"_s, makeScriptFunctionInvoker<Scripting::msToString>());

    registerFunction(u"if"_s, makeScriptFunctionInvoker<Scripting::cif>());
    registerFunction(u"if2"_s, makeScriptFunctionInvoker<Scripting::cif2>());
    registerFunction(u"if3"_s, makeScriptFunctionInvoker<Scripting::cif3>());
    registerFunction(u"ifgreater"_s, makeScriptFunctionInvoker<Scripting::ifgreater>());
    registerFunction(u"iflonger"_s, makeScriptFunctionInvoker<Scripting::iflonger>());
    registerFunction(u"ifequal"_s, makeScriptFunctionInvoker<Scripting::ifequal>());
}

QString ScriptRegistry::getBitrate(const Track& track) const
{
    int bitrate = track.bitrate();
    if(const auto* environment = playbackEnvironment(m_context); environment != nullptr && environment->bitrate() > 0) {
        bitrate = environment->bitrate();
    }
    return bitrate > 0 ? QString::number(bitrate) : QString{};
}

QString ScriptRegistry::playlistDuration(const TrackList& tracks) const
{
    const auto* environment = playbackEnvironment(m_context);
    if(!environment || tracks.empty()) {
        return {};
    }

    const int currentTrackIndex = m_context.environment && m_context.environment->playlistEnvironment()
                                    ? m_context.environment->playlistEnvironment()->currentPlaylistTrackIndex()
                                    : -1;
    if(currentTrackIndex < 0 || std::cmp_greater_equal(currentTrackIndex, tracks.size())) {
        return {};
    }
    uint64_t total{0};

    for(auto i{0}; i <= currentTrackIndex; ++i) {
        total += tracks[i].duration();
    }

    total -= environment->currentTrackDuration();
    total += environment->currentPosition();

    return Utils::msToString(total);
}

void ScriptRegistry::clearTrackListCache()
{
    m_trackListCache = {};
}

VariableKind ScriptRegistry::resolveVariableInternal(const QString& var) const
{
    if(const auto it = m_customVariableKinds.find(var); it != m_customVariableKinds.cend()) {
        return it->second;
    }

    return resolveBuiltInVariableKind(var);
}

ScriptResult ScriptRegistry::valueInternal(VariableKind kind, const QString& var, const ScriptSubject& subject) const
{
    if(subject.track) {
        return valueForTrack(kind, var, *subject.track, subject.playlist ? subject.playlist : m_context.playlist);
    }

    if(subject.tracks) {
        return valueForTrackList(kind, var, *subject.tracks, subject.playlist ? subject.playlist : m_context.playlist);
    }

    if(subject.playlist) {
        return valueForPlaylist(kind, var, *subject.playlist);
    }

    return {};
}

ScriptResult ScriptRegistry::valueInternal(const QString& var, const ScriptSubject& subject) const
{
    if(var.isEmpty()) {
        return {};
    }

    return value(resolveBuiltInVariableKind(var), var, subject);
}

ScriptResult ScriptRegistry::calculateResult(const ScriptRegistry::FuncRet& funcRet) const
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

    applyOutputPolicy(m_context, result);
    return result;
}

ScriptResult ScriptRegistry::valueForTrack(VariableKind kind, const QString& var, const Track& track,
                                           const Playlist* playlist) const
{
    if(const auto it = m_customVariables.find(kind); it != m_customVariables.cend()) {
        return it->second(makeContext(m_context, &track, nullptr, playlist), var);
    }

    if(isTrackListVariableKind(kind)) {
        if(trackListContextPolicy(m_context) == TrackListContextPolicy::Fallback) {
            if(const auto* tracks = contextTrackList(m_context); tracks != nullptr) {
                return value(kind, var, makeScriptSubject(*tracks));
            }
            if(playlist) {
                return value(kind, var, makeScriptSubject(playlist->tracks()));
            }
        }
        return unavailableTrackListResult(m_context, var);
    }

    if(const auto value = trackMetadataValue(kind, track, *this); value.has_value()) {
        return calculateResult(*value);
    }
    if(const auto value = playbackVariableValue(kind, *this); value.has_value()) {
        return calculateResult(*value);
    }
    if(const auto value = libraryVariableValue(kind, track, *this); value.has_value()) {
        return calculateResult(*value);
    }

    return calculateResult(track.extraTag(var));
}

ScriptResult ScriptRegistry::valueForTrackList(VariableKind kind, const QString& var, const TrackList& tracks,
                                               const Playlist* playlist) const
{
    if(const auto it = m_customVariables.find(kind); it != m_customVariables.cend()) {
        return it->second(makeContext(m_context, nullptr, &tracks, playlist), var);
    }

    if(const auto value = trackListValue(kind, tracks, *this); value.has_value()) {
        return calculateResult(*value);
    }

    if(tracks.empty()) {
        return {};
    }

    return value(kind, var, makeScriptSubject(tracks.front()));
}

ScriptResult ScriptRegistry::valueForPlaylist(VariableKind kind, const QString& var, const Playlist& playlist) const
{
    if(const auto it = m_customVariables.find(kind); it != m_customVariables.cend()) {
        return it->second(makeContext(m_context, nullptr, nullptr, &playlist), var);
    }

    if(var.isEmpty()) {
        return {};
    }

    if(isTrackListVariableKind(kind)) {
        switch(kind) {
            case VariableKind::TrackCount:
            case VariableKind::Playtime:
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

const ScriptRegistry::TrackListAggregateCache& ScriptRegistry::cachedTrackListValues(const TrackList& tracks) const
{
    const int trackCount      = static_cast<int>(tracks.size());
    const auto* environment   = playbackEnvironment(m_context);
    const int currentPosition = environment ? static_cast<int>(environment->currentPosition()) : -1;
    const int playlistIndex   = m_context.environment && m_context.environment->playlistEnvironment()
                                  ? m_context.environment->playlistEnvironment()->currentPlaylistTrackIndex()
                                  : -1;

    if(m_trackListCache.tracks != &tracks || m_trackListCache.trackCount != trackCount
       || m_trackListCache.currentPosition != currentPosition || m_trackListCache.playlistIndex != playlistIndex) {
        m_trackListCache.tracks          = &tracks;
        m_trackListCache.trackCount      = trackCount;
        m_trackListCache.currentPosition = currentPosition;
        m_trackListCache.playlistIndex   = playlistIndex;
        m_trackListCache.playtime        = Scripting::playtime(tracks);
        m_trackListCache.genres          = Scripting::genres(tracks);
        m_trackListCache.playlistElapsed = playlistDuration(tracks);
    }

    return m_trackListCache;
}

ScriptRegistry::ScriptRegistry()
{
    addDefaultFunctions();
    registerFunction(u"info"_s, makeScriptFunctionInvoker<trackInfo>());
    registerFunction(u"meta"_s, makeScriptFunctionInvoker<trackMeta>());
}

ScriptRegistry::~ScriptRegistry() = default;

void ScriptRegistry::addProvider(const ScriptVariableProvider& provider)
{
    for(const auto& variable : provider.variables()) {
        registerVariable(variable.kind, variable.name, variable.invoker);
    }
}

void ScriptRegistry::addProvider(const ScriptFunctionProvider& provider)
{
    for(const auto& function : provider.functions()) {
        registerFunction(function.name, function.invoker);
    }
}

VariableKind ScriptRegistry::resolveVariable(const QString& var) const
{
    return resolveVariableInternal(var);
}

ScriptFunctionId ScriptRegistry::resolveFunctionId(const QString& func) const
{
    const QString key = func.toLower();
    if(const auto it = m_functionIds.find(key); it != m_functionIds.cend()) {
        return it->second;
    }
    return InvalidScriptFunctionId;
}

bool ScriptRegistry::isVariable(const QString& var, const Track& track) const
{
    if(resolveBuiltInVariableKind(var) != VariableKind::Generic) {
        return true;
    }

    return track.hasExtraTag(var);
}

bool ScriptRegistry::isVariable(const QString& var, const TrackList& tracks) const
{
    const bool isListVariable = isTrackListVariableKind(resolveBuiltInVariableKind(var));
    if(isListVariable) {
        return true;
    }

    if(tracks.empty()) {
        return false;
    }

    return isVariable(var, tracks.front());
}

bool ScriptRegistry::isFunction(const QString& func) const
{
    const QString key = func.toLower();
    return m_functionIds.contains(key);
}

ScriptResult ScriptRegistry::value(VariableKind kind, const QString& var, const ScriptSubject& subject) const
{
    return valueInternal(kind, var, subject);
}

ScriptResult ScriptRegistry::value(const QString& var, const ScriptSubject& subject) const
{
    return valueInternal(var, subject);
}

ScriptResult ScriptRegistry::function(const QString& func, const ScriptValueList& args,
                                      const ScriptSubject& subject) const
{
    if(func.isEmpty()) {
        return {};
    }

    return function(resolveFunctionId(func), func, args, subject);
}

ScriptResult ScriptRegistry::function(ScriptFunctionId functionId, const QString& func, const ScriptValueList& args,
                                      const ScriptSubject& subject) const
{
    const auto* invoker = functionInvoker(functionId);
    if(!invoker && !func.isEmpty()) {
        functionId = resolveFunctionId(func);
        invoker    = functionInvoker(functionId);
    }
    if(!invoker || !(*invoker)) {
        return {};
    }

    const Track* track       = subject.track;
    const TrackList* tracks  = subject.tracks ? subject.tracks : m_context.tracks;
    const Playlist* playlist = subject.playlist ? subject.playlist : m_context.playlist;

    if(track == nullptr && subject.tracks != nullptr) {
        if(subject.tracks->empty()) {
            return {};
        }
        track  = &subject.tracks->front();
        tracks = subject.tracks;
    }
    else if(track == nullptr && subject.playlist != nullptr) {
        if(subject.playlist->tracks().empty()) {
            return {};
        }

        if(const auto playlistTrack = subject.playlist->currentTrack(); playlistTrack.isValid()) {
            track = &playlistTrack;
        }

        playlist = subject.playlist;
    }

    const ScriptFunctionCallContext call{
        .context = &m_context,
        .args    = args,
        .subject = {.track = track, .tracks = tracks, .playlist = playlist},
    };
    return (*invoker)(call);
}

ScriptContext ScriptRegistry::currentContext() const
{
    return m_context;
}

void ScriptRegistry::setContext(const ScriptContext& context)
{
    m_context = context;
    clearTrackListCache();
}

void ScriptRegistry::registerVariable(VariableKind kind, const QString& name, VariableInvoker invoker)
{
    const QString upperName = name.toUpper();

    m_customVariableKinds[upperName] = kind;
    m_customVariables[kind]          = invoker;
}

ScriptFunctionId ScriptRegistry::registerFunction(const QString& name, FunctionInvoker invoker)
{
    const QString key = name.toLower();

    if(const auto it = m_functionIds.find(key); it != m_functionIds.cend()) {
        m_functions[static_cast<size_t>(it->second - 1)] = {.name = key, .invoker = invoker};
        return it->second;
    }

    const auto functionId = static_cast<ScriptFunctionId>(m_functions.size() + 1);
    m_functionIds.emplace(key, functionId);
    m_functions.push_back({.name = key, .invoker = invoker});

    return functionId;
}

const ScriptRegistry::FunctionInvoker* ScriptRegistry::functionInvoker(ScriptFunctionId functionId) const
{
    if(functionId == InvalidScriptFunctionId || std::cmp_greater(functionId, m_functions.size())) {
        return nullptr;
    }

    return &m_functions.at(static_cast<size_t>(functionId - 1)).invoker;
}
} // namespace Fooyin
