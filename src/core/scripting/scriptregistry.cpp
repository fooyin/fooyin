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

QString trackInfo(const Fooyin::Track& track, const QStringList& args)
{
    if(args.empty()) {
        return {};
    }

    const QString& tag = args.front();

    if(tag == u"codec") {
        return track.codec();
    }
    if(tag == u"samplerate") {
        return QString::number(track.sampleRate());
    }
    if(tag == u"bitrate") {
        return QString::number(track.bitrate());
    }
    if(tag == u"channels") {
        return QString::number(track.channels());
    }
    if(tag == u"bitdepth") {
        return QString::number(track.bitDepth());
    }

    return {};
}

QString trackMeta(const Fooyin::Track& track, const QStringList& args)
{
    if(args.empty()) {
        return {};
    }

    const QString& tag = args.front();
    return track.metaValue(tag);
}

QString trackTitle(const Fooyin::Track& track)
{
    return !track.title().isEmpty() ? track.title() : track.filename();
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

    const QDateTime dateTime  = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(ms));
    QString formattedDateTime = dateTime.toString(u"yyyy-MM-dd HH:mm:ss");
    return formattedDateTime;
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

    LibraryManager* m_libraryManager{nullptr};
    PlayerController* m_playerController{nullptr};

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
    if(!m_playerController) {
        return;
    }

    m_playbackVars[QStringLiteral("playback_time")] = [this]() {
        return Utils::msToString(m_playerController->currentPosition());
    };
    m_playbackVars[QStringLiteral("playback_time_s")] = [this]() {
        return QString::number(m_playerController->currentPosition() / 1000);
    };
    m_playbackVars[QStringLiteral("playback_time_remaining")] = [this]() {
        return Utils::msToString(m_playerController->currentTrack().duration() - m_playerController->currentPosition());
    };
    m_playbackVars[QStringLiteral("playback_time_remaining_s")] = [this]() {
        return QString::number((m_playerController->currentTrack().duration() - m_playerController->currentPosition())
                               / 1000);
    };
}

void ScriptRegistryPrivate::addLibraryVars()
{
    if(!m_libraryManager) {
        return;
    }

    m_libraryVars[QStringLiteral("libraryname")] = [this](const Track& track) {
        if(const auto library = m_libraryManager->libraryInfo(track.libraryId())) {
            return library->name;
        }
        return QString{};
    };
    m_libraryVars[QStringLiteral("librarypath")] = [this](const Track& track) {
        if(const auto library = m_libraryManager->libraryInfo(track.libraryId())) {
            return library->path;
        }
        return QString{};
    };
}

void ScriptRegistryPrivate::addDefaultFunctions()
{
    m_funcs[QStringLiteral("add")] = Fooyin::Scripting::add;
    m_funcs[QStringLiteral("sub")] = Fooyin::Scripting::sub;
    m_funcs[QStringLiteral("mul")] = Fooyin::Scripting::mul;
    m_funcs[QStringLiteral("div")] = Fooyin::Scripting::div;
    m_funcs[QStringLiteral("min")] = Fooyin::Scripting::min;
    m_funcs[QStringLiteral("max")] = Fooyin::Scripting::max;
    m_funcs[QStringLiteral("mod")] = Fooyin::Scripting::mod;

    m_funcs[QStringLiteral("num")]            = Fooyin::Scripting::num;
    m_funcs[QStringLiteral("replace")]        = Fooyin::Scripting::replace;
    m_funcs[QStringLiteral("slice")]          = Fooyin::Scripting::slice;
    m_funcs[QStringLiteral("chop")]           = Fooyin::Scripting::chop;
    m_funcs[QStringLiteral("left")]           = Fooyin::Scripting::left;
    m_funcs[QStringLiteral("right")]          = Fooyin::Scripting::right;
    m_funcs[QStringLiteral("insert")]         = Fooyin::Scripting::insert;
    m_funcs[QStringLiteral("substr")]         = Fooyin::Scripting::substr;
    m_funcs[QStringLiteral("len")]            = Fooyin::Scripting::len;
    m_funcs[QStringLiteral("longest")]        = Fooyin::Scripting::longest;
    m_funcs[QStringLiteral("strcmp")]         = Fooyin::Scripting::strcmp;
    m_funcs[QStringLiteral("stricmp")]        = Fooyin::Scripting::stricmp;
    m_funcs[QStringLiteral("longer")]         = Fooyin::Scripting::longer;
    m_funcs[QStringLiteral("sep")]            = Fooyin::Scripting::sep;
    m_funcs[QStringLiteral("crlf")]           = Fooyin::Scripting::crlf;
    m_funcs[QStringLiteral("tab")]            = Fooyin::Scripting::tab;
    m_funcs[QStringLiteral("swapprefix")]     = Fooyin::Scripting::swapPrefix;
    m_funcs[QStringLiteral("stripprefix")]    = Fooyin::Scripting::stripPrefix;
    m_funcs[QStringLiteral("pad")]            = Fooyin::Scripting::pad;
    m_funcs[QStringLiteral("padright")]       = Fooyin::Scripting::padRight;
    m_funcs[QStringLiteral("repeat")]         = Fooyin::Scripting::repeat;
    m_funcs[QStringLiteral("trim")]           = Fooyin::Scripting::trim;
    m_funcs[QStringLiteral("lower")]          = Fooyin::Scripting::lower;
    m_funcs[QStringLiteral("upper")]          = Fooyin::Scripting::upper;
    m_funcs[QStringLiteral("abbr")]           = Fooyin::Scripting::abbr;
    m_funcs[QStringLiteral("caps")]           = Fooyin::Scripting::caps;
    m_funcs[QStringLiteral("directory")]      = Fooyin::Scripting::directory;
    m_funcs[QStringLiteral("directory_path")] = Fooyin::Scripting::directoryPath;
    m_funcs[QStringLiteral("ext")]            = Fooyin::Scripting::ext;
    m_funcs[QStringLiteral("filename")]       = Fooyin::Scripting::filename;
    m_funcs[QStringLiteral("progress")]       = Fooyin::Scripting::progress;
    m_funcs[QStringLiteral("progress2")]      = Fooyin::Scripting::progress2;

    m_funcs[QStringLiteral("timems")] = Fooyin::Scripting::msToString;

    m_funcs[QStringLiteral("if")]        = Fooyin::Scripting::cif;
    m_funcs[QStringLiteral("if2")]       = Fooyin::Scripting::cif2;
    m_funcs[QStringLiteral("ifgreater")] = Fooyin::Scripting::ifgreater;
    m_funcs[QStringLiteral("iflonger")]  = Fooyin::Scripting::iflonger;
    m_funcs[QStringLiteral("ifequal")]   = Fooyin::Scripting::ifequal;
}

void ScriptRegistryPrivate::addDefaultListFuncs()
{
    m_listProperties[QStringLiteral("trackcount")] = Fooyin::Scripting::trackCount;
    m_listProperties[QStringLiteral("playtime")]   = Fooyin::Scripting::playtime;
    m_listProperties[QStringLiteral("genres")]     = Fooyin::Scripting::genres;
}

void ScriptRegistryPrivate::addDefaultMetadata()
{
    using namespace Fooyin::Constants;

    m_metadata[QString::fromLatin1(MetaData::Title)]        = trackTitle;
    m_metadata[QString::fromLatin1(MetaData::Artist)]       = &Track::primaryArtist;
    m_metadata[QString::fromLatin1(MetaData::UniqueArtist)] = &Track::uniqueArtists;
    m_metadata[QString::fromLatin1(MetaData::Album)]        = &Track::album;
    m_metadata[QString::fromLatin1(MetaData::AlbumArtist)]  = &Track::primaryAlbumArtist;
    m_metadata[QString::fromLatin1(MetaData::Track)]        = &Track::trackNumber;
    m_metadata[QString::fromLatin1(MetaData::TrackTotal)]   = &Track::trackTotal;
    m_metadata[QString::fromLatin1(MetaData::Disc)]         = &Track::discNumber;
    m_metadata[QString::fromLatin1(MetaData::DiscTotal)]    = &Track::discTotal;
    m_metadata[QString::fromLatin1(MetaData::Genre)]        = &Track::genres;
    m_metadata[QString::fromLatin1(MetaData::Composer)]     = &Track::composer;
    m_metadata[QString::fromLatin1(MetaData::Performer)]    = &Track::performer;
    m_metadata[QString::fromLatin1(MetaData::Duration)]     = [](const Track& track) {
        const auto duration = track.duration();
        if(duration == 0) {
            return QString{};
        }
        return Utils::msToString(duration);
    };
    m_metadata[QString::fromLatin1(MetaData::DurationSecs)] = [](const Track& track) {
        const auto duration = track.duration();
        if(duration == 0) {
            return QString{};
        }
        return QString::number(duration / 1000);
    };
    m_metadata[QString::fromLatin1(MetaData::Comment)]  = &Track::comment;
    m_metadata[QString::fromLatin1(MetaData::Date)]     = &Track::date;
    m_metadata[QString::fromLatin1(MetaData::Year)]     = &Track::year;
    m_metadata[QString::fromLatin1(MetaData::FileSize)] = &Track::fileSize;
    m_metadata[QString::fromLatin1(MetaData::Bitrate)]  = [](const Track& track) {
        return track.bitrate() > 0 ? QStringLiteral("%1 kbps").arg(track.bitrate()) : QString{};
    };
    m_metadata[QString::fromLatin1(MetaData::SampleRate)] = [](const Track& track) {
        return track.sampleRate() > 0 ? QStringLiteral("%1 Hz").arg(track.sampleRate()) : QString{};
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
    m_metadata[QString::fromLatin1(MetaData::PlayCount)] = &Track::playCount;
    m_metadata[QString::fromLatin1(MetaData::Rating)]    = &Track::ratingStars;
    m_metadata[QString::fromLatin1(MetaData::Codec)]     = [](const Track& track) {
        return !track.codec().isEmpty() ? track.codec() : track.extension().toUpper();
    };
    m_metadata[QString::fromLatin1(MetaData::Channels)]        = trackChannels;
    m_metadata[QString::fromLatin1(MetaData::AddedTime)]       = &Track::addedTime;
    m_metadata[QString::fromLatin1(MetaData::LastModified)]    = &Track::lastModified;
    m_metadata[QString::fromLatin1(MetaData::FilePath)]        = &Track::filepath;
    m_metadata[QString::fromLatin1(MetaData::RelativePath)]    = &Track::relativePath;
    m_metadata[QString::fromLatin1(MetaData::FileName)]        = &Track::filename;
    m_metadata[QString::fromLatin1(MetaData::Extension)]       = &Track::extension;
    m_metadata[QString::fromLatin1(MetaData::FileNameWithExt)] = &Track::filenameExt;
    m_metadata[QString::fromLatin1(MetaData::Directory)]       = &Track::directory;
    m_metadata[QString::fromLatin1(MetaData::Path)]            = &Track::path;

    m_setMetadata[QString::fromLatin1(MetaData::Title)]       = generateSetFunc(&Track::setTitle);
    m_setMetadata[QString::fromLatin1(MetaData::Artist)]      = generateSetFunc(&Track::setArtists);
    m_setMetadata[QString::fromLatin1(MetaData::Album)]       = generateSetFunc(&Track::setAlbum);
    m_setMetadata[QString::fromLatin1(MetaData::AlbumArtist)] = generateSetFunc(&Track::setAlbumArtists);
    m_setMetadata[QString::fromLatin1(MetaData::Track)]       = generateSetFunc(&Track::setTrackNumber);
    m_setMetadata[QString::fromLatin1(MetaData::TrackTotal)]  = generateSetFunc(&Track::setTrackTotal);
    m_setMetadata[QString::fromLatin1(MetaData::Disc)]        = generateSetFunc(&Track::setDiscNumber);
    m_setMetadata[QString::fromLatin1(MetaData::DiscTotal)]   = generateSetFunc(&Track::setDiscTotal);
    m_setMetadata[QString::fromLatin1(MetaData::Genre)]       = generateSetFunc(&Track::setGenres);
    m_setMetadata[QString::fromLatin1(MetaData::Composer)]    = generateSetFunc(&Track::setComposer);
    m_setMetadata[QString::fromLatin1(MetaData::Performer)]   = generateSetFunc(&Track::setPerformer);
    m_setMetadata[QString::fromLatin1(MetaData::Duration)]    = generateSetFunc(&Track::setDuration);
    m_setMetadata[QString::fromLatin1(MetaData::Comment)]     = generateSetFunc(&Track::setComment);
    m_setMetadata[QString::fromLatin1(MetaData::Rating)]      = generateSetFunc(&Track::setRatingStars);
    m_setMetadata[QString::fromLatin1(MetaData::Date)]        = generateSetFunc(&Track::setDate);
    m_setMetadata[QString::fromLatin1(MetaData::Year)]        = generateSetFunc(&Track::setYear);
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

ScriptRegistry::~ScriptRegistry() = default;

bool ScriptRegistry::isVariable(const QString& var, const Track& track) const
{
    return p->m_metadata.contains(var) || p->m_playbackVars.contains(var) || p->m_libraryVars.contains(var)
        || track.hasExtraTag(var.toUpper());
}

bool ScriptRegistry::isVariable(const QString& var, const TrackList& tracks) const
{
    if(isListVariable(var)) {
        return true;
    }

    if(tracks.empty()) {
        return false;
    }

    return isVariable(var, tracks.front()) || p->m_playbackVars.contains(var) || p->m_libraryVars.contains(var)
        || tracks.front().hasExtraTag(var.toUpper());
}

bool ScriptRegistry::isFunction(const QString& func) const
{
    return p->m_funcs.contains(func);
}

ScriptResult ScriptRegistry::value(const QString& var, const Track& track) const
{
    if(var.isEmpty() || !isVariable(var, track)) {
        return {};
    }

    if(p->m_metadata.contains(var)) {
        return calculateResult(p->m_metadata.at(var)(track));
    }
    if(p->m_playbackVars.contains(var)) {
        return calculateResult(p->m_playbackVars.at(var)());
    }
    if(p->m_libraryVars.contains(var)) {
        return calculateResult(p->m_libraryVars.at(var)(track));
    }

    return calculateResult(track.extraTag(var.toUpper()));
}

ScriptResult ScriptRegistry::value(const QString& var, const TrackList& tracks) const
{
    if(var.isEmpty() || !isVariable(var, tracks)) {
        return {};
    }

    if(p->m_listProperties.contains(var)) {
        return calculateResult(p->m_listProperties.at(var)(tracks));
    }

    if(!tracks.empty()) {
        if(p->m_metadata.contains(var)) {
            return calculateResult(p->m_metadata.at(var)(tracks.front()));
        }
        return calculateResult(tracks.front().extraTag(var.toUpper()));
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

    if(isVariable(var, track)) {
        p->m_setMetadata.at(var)(track, value);
    }
}

bool ScriptRegistry::isListVariable(const QString& var) const
{
    return p->m_listProperties.contains(var);
}

ScriptResult ScriptRegistry::calculateResult(ScriptRegistry::FuncRet funcRet)
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
    else if(auto* strVal = std::get_if<QString>(&funcRet)) {
        result.value = *strVal;
        result.cond  = !result.value.isEmpty();
    }
    else if(auto* strListVal = std::get_if<QStringList>(&funcRet)) {
        result.value = strListVal->empty() ? QString{} : strListVal->join(u"\037");
        result.cond  = !result.value.isEmpty();
    }

    return result;
}
} // namespace Fooyin
