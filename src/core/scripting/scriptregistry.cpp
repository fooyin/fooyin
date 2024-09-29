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
#include <utils/utils.h>

#include <QDateTime>
#include <QDir>

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
            return QStringLiteral("Mono");
        case(2):
            return QStringLiteral("Stereo");
        default:
            return QStringLiteral("%1ch").arg(track.channels());
    }
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
    void addDefaultListFuncs();
    void addDefaultMetadata();

    QString getBitrate(const Track& track) const;

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
    addDefaultListFuncs();
    addDefaultMetadata();
    addPlaybackVars();
    addLibraryVars();

    m_funcs.emplace(QStringLiteral("info"), trackInfo);
    m_funcs.emplace(QStringLiteral("meta"), trackMeta);
}

void ScriptRegistryPrivate::addPlaybackVars()
{
    m_playbackVars[QStringLiteral("PLAYBACK_TIME")] = [this]() {
        return Utils::msToString(m_playerController ? m_playerController->currentPosition() : 0);
    };
    m_playbackVars[QStringLiteral("PLAYBACK_TIME_s")] = [this]() {
        return QString::number(m_playerController ? m_playerController->currentPosition() / 1000 : 0);
    };
    m_playbackVars[QStringLiteral("PLAYBACK_TIME_REMAINING")] = [this]() {
        return Utils::msToString(m_playerController ? m_playerController->currentTrack().duration()
                                                          - m_playerController->currentPosition()
                                                    : 0);
    };
    m_playbackVars[QStringLiteral("PLAYBACK_TIME_REMAINING_S")] = [this]() {
        return QString::number(
            m_playerController
                ? (m_playerController->currentTrack().duration() - m_playerController->currentPosition()) / 1000
                : 0);
    };
}

void ScriptRegistryPrivate::addLibraryVars()
{
    if(!m_libraryManager) {
        return;
    }

    m_libraryVars[QStringLiteral("LIBRARYNAME")] = [this](const Track& track) {
        if(const auto library = m_libraryManager->libraryInfo(track.libraryId())) {
            return library->name;
        }
        return QString{};
    };
    m_libraryVars[QStringLiteral("LIBRARYPATH")] = [this](const Track& track) {
        if(const auto library = m_libraryManager->libraryInfo(track.libraryId())) {
            return library->path;
        }
        return QString{};
    };
    m_libraryVars[QStringLiteral("RELATIVEPATH")] = [this](const Track& track) {
        if(const auto library = m_libraryManager->libraryInfo(track.libraryId())) {
            return QDir{library->path}.relativeFilePath(track.prettyFilepath());
        }
        return QString{};
    };
}

void ScriptRegistryPrivate::addDefaultFunctions()
{
    m_funcs[QStringLiteral("add")]  = Scripting::add;
    m_funcs[QStringLiteral("sub")]  = Scripting::sub;
    m_funcs[QStringLiteral("mul")]  = Scripting::mul;
    m_funcs[QStringLiteral("div")]  = Scripting::div;
    m_funcs[QStringLiteral("min")]  = Scripting::min;
    m_funcs[QStringLiteral("max")]  = Scripting::max;
    m_funcs[QStringLiteral("mod")]  = Scripting::mod;
    m_funcs[QStringLiteral("rand")] = Scripting::rand;

    m_funcs[QStringLiteral("num")]            = Scripting::num;
    m_funcs[QStringLiteral("replace")]        = Scripting::replace;
    m_funcs[QStringLiteral("ascii")]          = Scripting::ascii;
    m_funcs[QStringLiteral("slice")]          = Scripting::slice;
    m_funcs[QStringLiteral("chop")]           = Scripting::chop;
    m_funcs[QStringLiteral("left")]           = Scripting::left;
    m_funcs[QStringLiteral("right")]          = Scripting::right;
    m_funcs[QStringLiteral("insert")]         = Scripting::insert;
    m_funcs[QStringLiteral("substr")]         = Scripting::substr;
    m_funcs[QStringLiteral("len")]            = Scripting::len;
    m_funcs[QStringLiteral("longest")]        = Scripting::longest;
    m_funcs[QStringLiteral("strcmp")]         = Scripting::strcmp;
    m_funcs[QStringLiteral("stricmp")]        = Scripting::stricmp;
    m_funcs[QStringLiteral("longer")]         = Scripting::longer;
    m_funcs[QStringLiteral("sep")]            = Scripting::sep;
    m_funcs[QStringLiteral("crlf")]           = Scripting::crlf;
    m_funcs[QStringLiteral("tab")]            = Scripting::tab;
    m_funcs[QStringLiteral("swapprefix")]     = Scripting::swapPrefix;
    m_funcs[QStringLiteral("stripprefix")]    = Scripting::stripPrefix;
    m_funcs[QStringLiteral("pad")]            = Scripting::pad;
    m_funcs[QStringLiteral("padright")]       = Scripting::padRight;
    m_funcs[QStringLiteral("repeat")]         = Scripting::repeat;
    m_funcs[QStringLiteral("trim")]           = Scripting::trim;
    m_funcs[QStringLiteral("lower")]          = Scripting::lower;
    m_funcs[QStringLiteral("upper")]          = Scripting::upper;
    m_funcs[QStringLiteral("abbr")]           = Scripting::abbr;
    m_funcs[QStringLiteral("caps")]           = Scripting::caps;
    m_funcs[QStringLiteral("directory")]      = Scripting::directory;
    m_funcs[QStringLiteral("directory_path")] = Scripting::directoryPath;
    m_funcs[QStringLiteral("ext")]            = Scripting::ext;
    m_funcs[QStringLiteral("filename")]       = Scripting::filename;
    m_funcs[QStringLiteral("progress")]       = Scripting::progress;
    m_funcs[QStringLiteral("progress2")]      = Scripting::progress2;

    m_funcs[QStringLiteral("timems")] = Scripting::msToString;

    m_funcs[QStringLiteral("if")]        = Scripting::cif;
    m_funcs[QStringLiteral("if2")]       = Scripting::cif2;
    m_funcs[QStringLiteral("ifgreater")] = Scripting::ifgreater;
    m_funcs[QStringLiteral("iflonger")]  = Scripting::iflonger;
    m_funcs[QStringLiteral("ifequal")]   = Scripting::ifequal;
}

void ScriptRegistryPrivate::addDefaultListFuncs()
{
    m_listProperties[QStringLiteral("TRACKCOUNT")] = Scripting::trackCount;
    m_listProperties[QStringLiteral("PLAYTIME")]   = Scripting::playtime;
    m_listProperties[QStringLiteral("GENRES")]     = Scripting::genres;
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
    m_metadata[QString::fromLatin1(MetaData::Comment)]  = &Track::comment;
    m_metadata[QString::fromLatin1(MetaData::Date)]     = &Track::date;
    m_metadata[QString::fromLatin1(MetaData::Year)]     = &Track::year;
    m_metadata[QString::fromLatin1(MetaData::FileSize)] = &Track::fileSize;
    m_metadata[QString::fromLatin1(MetaData::Bitrate)]  = [this](const Track& track) {
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
        return track.tagType(QStringLiteral(" | "));
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
        return calculateResult(p->m_listProperties.at(variable)({track}));
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
