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

#include <core/scripting/scriptregistry.h>

#include "functions/controlfuncs.h"
#include "functions/mathfuncs.h"
#include "functions/stringfuncs.h"
#include "functions/timefuncs.h"
#include "functions/tracklistfuncs.h"
#include "library/librarymanager.h"

#include <core/constants.h>
#include <core/player/playercontroller.h>
#include <core/track.h>
#include <utils/audioutils.h>
#include <utils/stringutils.h>
#include <utils/utils.h>

#include <QDateTime>
#include <QDir>

using namespace Qt::StringLiterals;

namespace {
using NativeFunc          = std::function<QString(const QStringList&)>;
using NativeVoidFunc      = std::function<QString()>;
using NativeTrackFunc     = std::function<QString(const Fooyin::Track&, const QStringList&)>;
using NativeTrackVoidFunc = std::function<QString(const Fooyin::Track&)>;
using NativeBoolFunc      = std::function<Fooyin::ScriptResult(const QStringList&)>;
using NativeCondFunc      = std::function<Fooyin::ScriptResult(const Fooyin::ScriptValueList&)>;
using Func                = std::variant<NativeFunc, NativeVoidFunc, NativeTrackFunc, NativeBoolFunc, NativeCondFunc>;

using TrackFunc     = std::function<Fooyin::ScriptRegistry::FuncRet(const Fooyin::Track&)>;
using TrackSetFunc  = std::function<void(Fooyin::Track&, const Fooyin::ScriptRegistry::FuncRet&)>;
using TrackListFunc = std::function<Fooyin::ScriptRegistry::FuncRet(const Fooyin::TrackList&)>;

template <typename FuncType>
auto generateSetFunc(FuncType func)
{
    return [func](Fooyin::Track& track, Fooyin::ScriptRegistry::FuncRet arg) {
        if constexpr(std::is_same_v<FuncType, void (Fooyin::Track::*)(int)>) {
            if(const auto* value = std::get_if<int>(&arg)) {
                (track.*func)(*value);
            }
        }
        else if constexpr(std::is_same_v<FuncType, void (Fooyin::Track::*)(uint64_t)>) {
            if(const auto* value = std::get_if<uint64_t>(&arg)) {
                (track.*func)(*value);
            }
        }
        else if constexpr(std::is_same_v<FuncType, void (Fooyin::Track::*)(float)>) {
            if(const auto* value = std::get_if<float>(&arg)) {
                (track.*func)(*value);
            }
        }
        else if constexpr(std::is_same_v<FuncType, void (Fooyin::Track::*)(const QString&)>) {
            if(const auto* value = std::get_if<QString>(&arg)) {
                (track.*func)(*value);
            }
        }
        else if constexpr(std::is_same_v<FuncType, void (Fooyin::Track::*)(const QStringList&)>) {
            if(const auto* value = std::get_if<QStringList>(&arg)) {
                (track.*func)(*value);
            }
        }
    };
}

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
} // namespace

namespace Fooyin {
class ScriptRegistryPrivate
{
public:
    explicit ScriptRegistryPrivate(LibraryManager* libraryManager, PlayerController* playerController);

    void addPlaybackVars();
    void addLibraryVars();
    void addDefaultFunctions();
    void addPlaylistVars();
    void addDefaultMetadata();

    QString getBitrate(const Track& track) const;
    QString playlistDuration(const TrackList& tracks) const;

    LibraryManager* m_libraryManager{nullptr};
    PlayerController* m_playerController{nullptr};

    bool m_useVariousArtists{false};

    std::unordered_map<QString, TrackFunc> m_metadata;
    std::unordered_map<QString, TrackSetFunc> m_setMetadata;
    std::unordered_map<QString, TrackListFunc> m_listProperties;
    std::unordered_map<QString, Func> m_funcs;
    std::unordered_map<QString, NativeVoidFunc> m_playbackVars;
    std::unordered_map<QString, NativeTrackVoidFunc> m_libraryVars;
};

ScriptRegistryPrivate::ScriptRegistryPrivate(LibraryManager* libraryManager, PlayerController* playerController)
    : m_libraryManager{libraryManager}
    , m_playerController{playerController}
{
    addDefaultFunctions();
    addPlaylistVars();
    addDefaultMetadata();
    addPlaybackVars();
    addLibraryVars();

    m_funcs.emplace(u"info"_s, trackInfo);
    m_funcs.emplace(u"meta"_s, trackMeta);
}

void ScriptRegistryPrivate::addPlaybackVars()
{
    m_playbackVars[u"PLAYBACK_TIME"_s] = [this]() {
        return Utils::msToString(m_playerController ? m_playerController->currentPosition() : 0);
    };
    m_playbackVars[u"PLAYBACK_TIME_S"_s] = [this]() {
        return QString::number(m_playerController ? m_playerController->currentPosition() / 1000 : 0);
    };
    m_playbackVars[u"PLAYBACK_TIME_REMAINING"_s] = [this]() {
        return Utils::msToString(m_playerController ? m_playerController->currentTrack().duration()
                                                          - m_playerController->currentPosition()
                                                    : 0);
    };
    m_playbackVars[u"PLAYBACK_TIME_REMAINING_S"_s] = [this]() {
        return QString::number(
            m_playerController
                ? (m_playerController->currentTrack().duration() - m_playerController->currentPosition()) / 1000
                : 0);
    };
    m_playbackVars[u"ISPLAYING"_s] = [this]() {
        return QString::number(
            static_cast<int>(m_playerController && m_playerController->playState() == Player::PlayState::Playing));
    };
    m_playbackVars[u"ISPAUSED"_s] = [this]() {
        return QString::number(
            static_cast<int>(m_playerController && m_playerController->playState() == Player::PlayState::Paused));
    };
}

void ScriptRegistryPrivate::addLibraryVars()
{
    if(!m_libraryManager) {
        return;
    }

    m_libraryVars[u"LIBRARYNAME"_s] = [this](const Track& track) {
        if(const auto library = m_libraryManager->libraryInfo(track.libraryId())) {
            return library->name;
        }
        return QString{};
    };
    m_libraryVars[u"LIBRARYPATH"_s] = [this](const Track& track) {
        if(const auto library = m_libraryManager->libraryInfo(track.libraryId())) {
            return library->path;
        }
        return QString{};
    };
    m_libraryVars[u"RELATIVEPATH"_s] = [this](const Track& track) {
        if(const auto library = m_libraryManager->libraryInfo(track.libraryId())) {
            return QDir{library->path}.relativeFilePath(track.prettyFilepath());
        }
        return QString{};
    };
}

void ScriptRegistryPrivate::addDefaultFunctions()
{
    m_funcs[u"add"_s]   = Scripting::add;
    m_funcs[u"sub"_s]   = Scripting::sub;
    m_funcs[u"mul"_s]   = Scripting::mul;
    m_funcs[u"div"_s]   = Scripting::div;
    m_funcs[u"min"_s]   = Scripting::min;
    m_funcs[u"max"_s]   = Scripting::max;
    m_funcs[u"mod"_s]   = Scripting::mod;
    m_funcs[u"rand"_s]  = Scripting::rand;
    m_funcs[u"round"_s] = Scripting::round;

    m_funcs[u"num"_s]            = Scripting::num;
    m_funcs[u"replace"_s]        = Scripting::replace;
    m_funcs[u"ascii"_s]          = Scripting::ascii;
    m_funcs[u"slice"_s]          = Scripting::slice;
    m_funcs[u"chop"_s]           = Scripting::chop;
    m_funcs[u"left"_s]           = Scripting::left;
    m_funcs[u"right"_s]          = Scripting::right;
    m_funcs[u"insert"_s]         = Scripting::insert;
    m_funcs[u"substr"_s]         = Scripting::substr;
    m_funcs[u"strstr"_s]         = Scripting::strstr;
    m_funcs[u"stristr"_s]        = Scripting::stristr;
    m_funcs[u"strstrlast"_s]     = Scripting::strstrLast;
    m_funcs[u"stristrlast"_s]    = Scripting::stristrLast;
    m_funcs[u"split"_s]          = Scripting::split;
    m_funcs[u"len"_s]            = Scripting::len;
    m_funcs[u"longest"_s]        = Scripting::longest;
    m_funcs[u"strcmp"_s]         = Scripting::strcmp;
    m_funcs[u"stricmp"_s]        = Scripting::stricmp;
    m_funcs[u"longer"_s]         = Scripting::longer;
    m_funcs[u"sep"_s]            = Scripting::sep;
    m_funcs[u"crlf"_s]           = Scripting::crlf;
    m_funcs[u"tab"_s]            = Scripting::tab;
    m_funcs[u"swapprefix"_s]     = Scripting::swapPrefix;
    m_funcs[u"stripprefix"_s]    = Scripting::stripPrefix;
    m_funcs[u"pad"_s]            = Scripting::pad;
    m_funcs[u"padright"_s]       = Scripting::padRight;
    m_funcs[u"repeat"_s]         = Scripting::repeat;
    m_funcs[u"trim"_s]           = Scripting::trim;
    m_funcs[u"lower"_s]          = Scripting::lower;
    m_funcs[u"upper"_s]          = Scripting::upper;
    m_funcs[u"abbr"_s]           = Scripting::abbr;
    m_funcs[u"caps"_s]           = Scripting::caps;
    m_funcs[u"directory"_s]      = Scripting::directory;
    m_funcs[u"directory_path"_s] = Scripting::directoryPath;
    m_funcs[u"elide_end"_s]      = Scripting::elideEnd;
    m_funcs[u"elide_mid"_s]      = Scripting::elideMid;
    m_funcs[u"ext"_s]            = Scripting::ext;
    m_funcs[u"filename"_s]       = Scripting::filename;
    m_funcs[u"progress"_s]       = Scripting::progress;
    m_funcs[u"progress2"_s]      = Scripting::progress2;

    m_funcs[u"timems"_s] = Scripting::msToString;

    m_funcs[u"if"_s]        = Scripting::cif;
    m_funcs[u"if2"_s]       = Scripting::cif2;
    m_funcs[u"ifgreater"_s] = Scripting::ifgreater;
    m_funcs[u"iflonger"_s]  = Scripting::iflonger;
    m_funcs[u"ifequal"_s]   = Scripting::ifequal;
}

void ScriptRegistryPrivate::addPlaylistVars()
{
    m_listProperties[u"TRACKCOUNT"_s]        = Scripting::trackCount;
    m_listProperties[u"PLAYTIME"_s]          = Scripting::playtime;
    m_listProperties[u"PLAYLIST_DURATION"_s] = Scripting::playtime;
    if(m_playerController) {
        m_listProperties[u"PLAYLIST_ELAPSED"_s] = [this](const TrackList& tracks) {
            return playlistDuration(tracks);
        };
    }
    m_listProperties[u"GENRES"_s] = Scripting::genres;
}

void ScriptRegistryPrivate::addDefaultMetadata()
{
    using namespace Fooyin::Constants;

    m_metadata[QString::fromLatin1(MetaData::Title)]        = &Track::effectiveTitle;
    m_metadata[QString::fromLatin1(MetaData::Artist)]       = &Track::primaryArtist;
    m_metadata[QString::fromLatin1(MetaData::UniqueArtist)] = &Track::uniqueArtists;
    m_metadata[QString::fromLatin1(MetaData::Album)]        = &Track::album;
    m_metadata[QString::fromLatin1(MetaData::AlbumArtist)]  = [this](const Track& track) {
        return track.effectiveAlbumArtist(m_useVariousArtists);
    };
    m_metadata[QString::fromLatin1(MetaData::Track)]      = &Track::trackNumber;
    m_metadata[QString::fromLatin1(MetaData::TrackTotal)] = &Track::trackTotal;
    m_metadata[QString::fromLatin1(MetaData::Disc)]       = &Track::discNumber;
    m_metadata[QString::fromLatin1(MetaData::DiscTotal)]  = &Track::discTotal;
    m_metadata[QString::fromLatin1(MetaData::Genre)]      = &Track::genres;
    m_metadata[QString::fromLatin1(MetaData::Composer)]   = &Track::composer;
    m_metadata[QString::fromLatin1(MetaData::Performer)]  = &Track::performer;
    m_metadata[QString::fromLatin1(MetaData::Duration)]   = [](const Track& track) {
        const auto duration = track.duration();
        return duration == 0 ? QString{} : Utils::msToString(duration);
    };
    m_metadata[QString::fromLatin1(MetaData::DurationSecs)] = [](const Track& track) {
        const auto duration = track.duration();
        return duration == 0 ? QString{} : QString::number(duration / 1000);
    };
    m_metadata[QString::fromLatin1(MetaData::DurationMSecs)] = [](const Track& track) {
        const auto duration = track.duration();
        return duration == 0 ? QString{} : QString::number(duration);
    };
    m_metadata[QString::fromLatin1(MetaData::Comment)]         = &Track::comment;
    m_metadata[QString::fromLatin1(MetaData::Date)]            = &Track::date;
    m_metadata[QString::fromLatin1(MetaData::Year)]            = &Track::year;
    m_metadata[QString::fromLatin1(MetaData::FileSize)]        = &Track::fileSize;
    m_metadata[QString::fromLatin1(MetaData::FileSizeNatural)] = [](const Track& track) {
        return Utils::formatFileSize(track.fileSize());
    };
    m_metadata[QString::fromLatin1(MetaData::Bitrate)] = [this](const Track& track) {
        return getBitrate(track);
    };
    m_metadata[QString::fromLatin1(MetaData::SampleRate)] = [](const Track& track) {
        return track.sampleRate() > 0 ? QString::number(track.sampleRate()) : QString{};
    };
    m_metadata[QString::fromLatin1(MetaData::BitDepth)] = [](const Track& track) {
        return track.bitDepth() > 0 ? track.bitDepth() : -1;
    };
    m_metadata[QString::fromLatin1(MetaData::FirstPlayed)] = [](const Track& track) {
        return formatDateTime(track.firstPlayed());
    };
    m_metadata[QString::fromLatin1(MetaData::LastPlayed)] = [](const Track& track) {
        return formatDateTime(track.lastPlayed());
    };
    m_metadata[QString::fromLatin1(MetaData::PlayCount)]    = &Track::playCount;
    m_metadata[QString::fromLatin1(MetaData::Rating)]       = &Track::rating;
    m_metadata[QString::fromLatin1(MetaData::RatingStars)]  = &Track::ratingStars;
    m_metadata[QString::fromLatin1(MetaData::RatingEditor)] = &Track::ratingStars;
    m_metadata[QString::fromLatin1(MetaData::Codec)]        = [](const Track& track) {
        return !track.codec().isEmpty() ? track.codec() : track.extension().toUpper();
    };
    m_metadata[QString::fromLatin1(MetaData::CodecProfile)] = &Track::codecProfile;
    m_metadata[QString::fromLatin1(MetaData::Tool)]         = &Track::tool;
    m_metadata[QString::fromLatin1(MetaData::TagType)]      = [](const Track& track) {
        return track.tagType(u" | "_s);
    };
    m_metadata[QString::fromLatin1(MetaData::Encoding)]  = &Track::encoding;
    m_metadata[QString::fromLatin1(MetaData::Channels)]  = trackChannels;
    m_metadata[QString::fromLatin1(MetaData::AddedTime)] = [](const Track& track) {
        return formatDateTime(track.addedTime());
    };
    m_metadata[QString::fromLatin1(MetaData::LastModified)] = [](const Track& track) {
        return formatDateTime(track.lastModified());
    };
    m_metadata[QString::fromLatin1(MetaData::FilePath)]        = &Track::filepath;
    m_metadata[QString::fromLatin1(MetaData::FileName)]        = &Track::filename;
    m_metadata[QString::fromLatin1(MetaData::Extension)]       = &Track::extension;
    m_metadata[QString::fromLatin1(MetaData::FileNameWithExt)] = &Track::filenameExt;
    m_metadata[QString::fromLatin1(MetaData::Directory)]       = &Track::directory;
    m_metadata[QString::fromLatin1(MetaData::Path)]            = &Track::path;
    m_metadata[QString::fromLatin1(MetaData::Subsong)]         = &Track::subsong;
    m_metadata[QString::fromLatin1(MetaData::RGTrackGain)]     = [](const Track& track) {
        return formatGain(track.rgTrackGain());
    };
    m_metadata[QString::fromLatin1(MetaData::RGTrackPeak)]   = &Track::rgTrackPeak;
    m_metadata[QString::fromLatin1(MetaData::RGTrackPeakDB)] = [](const Track& track) {
        return formatPeak(track.rgTrackPeak());
    };
    m_metadata[QString::fromLatin1(MetaData::RGAlbumGain)] = [](const Track& track) {
        return formatGain(track.rgAlbumGain());
    };
    m_metadata[QString::fromLatin1(MetaData::RGAlbumPeak)]   = &Track::rgAlbumPeak;
    m_metadata[QString::fromLatin1(MetaData::RGAlbumPeakDB)] = [](const Track& track) {
        return formatPeak(track.rgAlbumPeak());
    };

    m_setMetadata[QString::fromLatin1(MetaData::Title)]        = generateSetFunc(&Track::setTitle);
    m_setMetadata[QString::fromLatin1(MetaData::Artist)]       = generateSetFunc(&Track::setArtists);
    m_setMetadata[QString::fromLatin1(MetaData::Album)]        = generateSetFunc(&Track::setAlbum);
    m_setMetadata[QString::fromLatin1(MetaData::AlbumArtist)]  = generateSetFunc(&Track::setAlbumArtists);
    m_setMetadata[QString::fromLatin1(MetaData::Track)]        = generateSetFunc(&Track::setTrackNumber);
    m_setMetadata[QString::fromLatin1(MetaData::TrackTotal)]   = generateSetFunc(&Track::setTrackTotal);
    m_setMetadata[QString::fromLatin1(MetaData::Disc)]         = generateSetFunc(&Track::setDiscNumber);
    m_setMetadata[QString::fromLatin1(MetaData::DiscTotal)]    = generateSetFunc(&Track::setDiscTotal);
    m_setMetadata[QString::fromLatin1(MetaData::Genre)]        = generateSetFunc(&Track::setGenres);
    m_setMetadata[QString::fromLatin1(MetaData::Composer)]     = generateSetFunc(&Track::setComposers);
    m_setMetadata[QString::fromLatin1(MetaData::Performer)]    = generateSetFunc(&Track::setPerformers);
    m_setMetadata[QString::fromLatin1(MetaData::Duration)]     = generateSetFunc(&Track::setDuration);
    m_setMetadata[QString::fromLatin1(MetaData::Comment)]      = generateSetFunc(&Track::setComment);
    m_setMetadata[QString::fromLatin1(MetaData::Rating)]       = generateSetFunc(&Track::setRating);
    m_setMetadata[QString::fromLatin1(MetaData::RatingStars)]  = generateSetFunc(&Track::setRatingStars);
    m_setMetadata[QString::fromLatin1(MetaData::RatingEditor)] = generateSetFunc(&Track::setRatingStars);
    m_setMetadata[QString::fromLatin1(MetaData::Date)]         = generateSetFunc(&Track::setDate);
    m_setMetadata[QString::fromLatin1(MetaData::Year)]         = generateSetFunc(&Track::setYear);
}

QString ScriptRegistryPrivate::getBitrate(const Track& track) const
{
    int bitrate = track.bitrate();
    if(m_playerController && m_playerController->bitrate() > 0) {
        bitrate = m_playerController->bitrate();
    }
    return bitrate > 0 ? QString::number(bitrate) : QString{};
}

QString ScriptRegistryPrivate::playlistDuration(const TrackList& tracks) const
{
    const auto currentTrack = m_playerController->currentPlaylistTrack();
    uint64_t total{0};

    for(auto i{0}; i <= currentTrack.indexInPlaylist; ++i) {
        total += tracks[i].duration();
    }

    total -= currentTrack.track.duration();
    total += m_playerController->currentPosition();

    return Utils::msToString(total);
}

ScriptRegistry::ScriptRegistry()
    : ScriptRegistry{nullptr, nullptr}
{ }

ScriptRegistry::ScriptRegistry(LibraryManager* libraryManager)
    : ScriptRegistry{libraryManager, nullptr}
{ }

ScriptRegistry::ScriptRegistry(PlayerController* playerController)
    : ScriptRegistry{nullptr, playerController}
{ }

ScriptRegistry::ScriptRegistry(LibraryManager* libraryManager, PlayerController* playerController)
    : p{std::make_unique<ScriptRegistryPrivate>(libraryManager, playerController)}
{ }

void ScriptRegistry::setUseVariousArtists(bool enabled)
{
    p->m_useVariousArtists = enabled;
}

ScriptRegistry::~ScriptRegistry() = default;

bool ScriptRegistry::isVariable(const QString& var, const Track& track) const
{
    const QString variable = var.toUpper();
    return p->m_metadata.contains(variable) || p->m_playbackVars.contains(variable)
        || p->m_libraryVars.contains(variable) || track.hasExtraTag(variable);
}

bool ScriptRegistry::isVariable(const QString& var, const TrackList& tracks) const
{
    if(isListVariable(var)) {
        return true;
    }

    if(tracks.empty()) {
        return false;
    }

    return isVariable(var, tracks.front());
}

bool ScriptRegistry::isFunction(const QString& func) const
{
    return p->m_funcs.contains(func);
}

ScriptResult ScriptRegistry::value(const QString& var, const Track& track) const
{
    if(var.isEmpty() || (!isVariable(var, track) && !isListVariable(var))) {
        return {};
    }

    const QString variable = var.toUpper();

    if(p->m_metadata.contains(variable)) {
        return calculateResult(p->m_metadata.at(variable)(track));
    }
    if(p->m_playbackVars.contains(variable)) {
        return calculateResult(p->m_playbackVars.at(variable)());
    }
    if(p->m_libraryVars.contains(variable)) {
        return calculateResult(p->m_libraryVars.at(variable)(track));
    }
    if(p->m_listProperties.contains(variable)) {
        return {.value = u"%%1%"_s.arg(var), .cond = true};
    }

    return calculateResult(track.extraTag(variable));
}

ScriptResult ScriptRegistry::value(const QString& var, const TrackList& tracks) const
{
    if(var.isEmpty() || !isVariable(var, tracks)) {
        return {};
    }

    const QString variable = var.toUpper();

    if(p->m_listProperties.contains(variable)) {
        return calculateResult(p->m_listProperties.at(variable)(tracks));
    }

    if(!tracks.empty()) {
        if(p->m_metadata.contains(variable)) {
            return calculateResult(p->m_metadata.at(variable)(tracks.front()));
        }
        return calculateResult(tracks.front().extraTag(variable.toUpper()));
    }

    return {};
}

ScriptResult ScriptRegistry::value(const QString& var, const Playlist& playlist) const
{
    const TrackList tracks = playlist.tracks();

    if(var.isEmpty() || !isVariable(var, tracks)) {
        return {};
    }

    const QString variable = var.toUpper();

    if(p->m_listProperties.contains(variable)) {
        return calculateResult(p->m_listProperties.at(variable)(tracks));
    }

    if(auto track = playlist.track(playlist.currentTrackIndex())) {
        return value(var, track.value());
    }

    return {};
}

ScriptResult ScriptRegistry::function(const QString& func, const ScriptValueList& args, const Track& track) const
{
    if(func.isEmpty() || !p->m_funcs.contains(func)) {
        return {};
    }

    const auto scriptFunc = p->m_funcs.at(func);
    if(std::holds_alternative<NativeFunc>(scriptFunc)) {
        const QString value = std::get<NativeFunc>(scriptFunc)(containerCast<QStringList>(args));
        return {.value = value, .cond = !value.isEmpty()};
    }
    if(std::holds_alternative<NativeVoidFunc>(scriptFunc)) {
        const QString value = std::get<NativeVoidFunc>(scriptFunc)();
        return {.value = value, .cond = !value.isEmpty()};
    }
    if(std::holds_alternative<NativeTrackFunc>(scriptFunc)) {
        const QString value = std::get<NativeTrackFunc>(scriptFunc)(track, containerCast<QStringList>(args));
        return {.value = value, .cond = !value.isEmpty()};
    }
    if(std::holds_alternative<NativeBoolFunc>(scriptFunc)) {
        return std::get<NativeBoolFunc>(scriptFunc)(containerCast<QStringList>(args));
    }
    if(std::holds_alternative<NativeCondFunc>(scriptFunc)) {
        return std::get<NativeCondFunc>(scriptFunc)(args);
    }

    return {};
}

ScriptResult ScriptRegistry::function(const QString& func, const ScriptValueList& args, const TrackList& tracks) const
{
    if(func.isEmpty() || !p->m_funcs.contains(func)) {
        return {};
    }

    if(tracks.empty()) {
        return {};
    }

    return function(func, args, tracks.front());
}

ScriptResult ScriptRegistry::function(const QString& func, const ScriptValueList& args, const Playlist& playlist) const
{
    if(func.isEmpty() || !p->m_funcs.contains(func)) {
        return {};
    }

    if(playlist.tracks().empty()) {
        return {};
    }

    return function(func, args, playlist.currentTrack());
}

void ScriptRegistry::setValue(const QString& var, const FuncRet& value, Track& track)
{
    if(var.isEmpty()) {
        return;
    }

    const QString tag = var.toUpper();

    if(p->m_setMetadata.contains(tag)) {
        p->m_setMetadata.at(tag)(track, value);
        return;
    }

    const auto setOrAddTag = [&](const auto& val) {
        if(track.hasExtraTag(tag)) {
            track.replaceExtraTag(tag, val);
        }
        else {
            track.addExtraTag(tag, val);
        }
    };

    if(const auto* strVal = std::get_if<QString>(&value)) {
        setOrAddTag(*strVal);
    }
    else if(const auto* listVal = std::get_if<QStringList>(&value)) {
        setOrAddTag(*listVal);
    }
}

bool ScriptRegistry::isListVariable(const QString& var) const
{
    return p->m_listProperties.contains(var.toUpper());
}

ScriptResult ScriptRegistry::calculateResult(ScriptRegistry::FuncRet funcRet) const
{
    ScriptResult result;

    if(auto* intVal = std::get_if<int>(&funcRet)) {
        result.value = QString::number(*intVal);
        result.cond  = (*intVal) >= 0;
    }
    else if(auto* uintVal = std::get_if<uint64_t>(&funcRet)) {
        result.value = QString::number(*uintVal);
        result.cond  = true;
    }
    else if(auto* floatVal = std::get_if<float>(&funcRet)) {
        result.value = QString::number(*floatVal);
        result.cond  = (*floatVal) >= 0;
    }
    else if(auto* strVal = std::get_if<QString>(&funcRet)) {
        result.value = *strVal;
        result.cond  = !result.value.isEmpty();
    }
    else if(auto* strListVal = std::get_if<QStringList>(&funcRet)) {
        result.value = strListVal->empty() ? QString{} : strListVal->join(QLatin1String{Constants::UnitSeparator});
        result.cond  = !result.value.isEmpty();
    }

    return result;
}
} // namespace Fooyin
