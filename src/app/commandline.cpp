/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include "commandline.h"

#include "version.h"

#include <QDataStream>
#include <QFile>
#include <QFileInfo>
#include <QObject>

#include <getopt.h>
#include <limits>

using namespace Qt::StringLiterals;

namespace {
enum CommandOption : uint16_t // NOLINT
{
    Volume = 256,
    VolumeUp,
    VolumeDown,
    Repeat,
    Shuffle,
};

#ifdef Q_OS_WIN
QString decodeName(wchar_t* opt)
{
    return QString::fromWCharArray(opt);
}
#else
QString decodeName(char* opt)
{
    return QFile::decodeName(opt);
}
#endif

std::optional<uint64_t> parseUnsigned(const QStringView value)
{
    if(value.isEmpty()) {
        return {};
    }

    uint64_t result{0};
    for(const QChar character : value) {
        if(!character.isDigit()) {
            return {};
        }

        const uint64_t digit = character.digitValue();
        if(result > (std::numeric_limits<uint64_t>::max() - digit) / 10) {
            return {};
        }
        result = (result * 10) + digit;
    }
    return result;
}

std::optional<uint64_t> multiply(const uint64_t value, const uint64_t multiplier)
{
    if(value > std::numeric_limits<uint64_t>::max() / multiplier) {
        return {};
    }
    return value * multiplier;
}

bool isRemoteUrl(const QUrl& url)
{
    return url.isValid() && !url.host().isEmpty() && (url.scheme() == u"http" || url.scheme() == u"https");
}

std::optional<CommandLine::RepeatMode> parseRepeatMode(const QStringView value)
{
    if(value == u"off") {
        return CommandLine::RepeatMode::Off;
    }
    if(value == u"playlist") {
        return CommandLine::RepeatMode::Playlist;
    }
    if(value == u"album") {
        return CommandLine::RepeatMode::Album;
    }
    if(value == u"track") {
        return CommandLine::RepeatMode::Track;
    }
    return {};
}

std::optional<CommandLine::ShuffleMode> parseShuffleMode(const QStringView value)
{
    if(value == u"off") {
        return CommandLine::ShuffleMode::Off;
    }
    if(value == u"tracks") {
        return CommandLine::ShuffleMode::Tracks;
    }
    if(value == u"albums") {
        return CommandLine::ShuffleMode::Albums;
    }
    if(value == u"random") {
        return CommandLine::ShuffleMode::Random;
    }
    return {};
}
} // namespace

CommandLine::CommandLine(int argc, char** argv)
    : m_argc{argc}
#ifdef Q_OS_WIN
    , m_argv{CommandLineToArgvW(GetCommandLineW(), &argc)}
#else
    , m_argv{argv}
#endif
    , m_seekDelta{0}
    , m_volume{0.0}
    , m_skipSingle{false}
    , m_playerAction{PlayerAction::None}
    , m_volumeAction{VolumeAction::None}
    , m_repeatMode{RepeatMode::Unchanged}
    , m_shuffleMode{ShuffleMode::Unchanged}
{ }

#ifdef Q_OS_WIN
#define FY_NATIVE_CLI_TEXT(value) L##value // NOLINT
#else
#define FY_NATIVE_CLI_TEXT(value) value // NOLINT
#endif

CommandLine::ParseResult CommandLine::parse()
{
    m_files.clear();
    m_message.clear();

    m_seekDelta    = 0;
    m_volume       = 0.0;
    m_skipSingle   = false;
    m_playerAction = PlayerAction::None;
    m_volumeAction = VolumeAction::None;
    m_repeatMode   = RepeatMode::Unchanged;
    m_shuffleMode  = ShuffleMode::Unchanged;

    optind = 1;
    opterr = 0;

    static constexpr option cmdOptions[] = {
        {.name = FY_NATIVE_CLI_TEXT("help"), .has_arg = no_argument, .flag = nullptr, .val = 'h'},
        {.name = FY_NATIVE_CLI_TEXT("version"), .has_arg = no_argument, .flag = nullptr, .val = 'v'},
        {.name = FY_NATIVE_CLI_TEXT("skip-single"), .has_arg = no_argument, .flag = nullptr, .val = 'x'},
        {.name = FY_NATIVE_CLI_TEXT("play-pause"), .has_arg = no_argument, .flag = nullptr, .val = 't'},
        {.name = FY_NATIVE_CLI_TEXT("play"), .has_arg = no_argument, .flag = nullptr, .val = 'p'},
        {.name = FY_NATIVE_CLI_TEXT("pause"), .has_arg = no_argument, .flag = nullptr, .val = 'u'},
        {.name = FY_NATIVE_CLI_TEXT("stop"), .has_arg = no_argument, .flag = nullptr, .val = 's'},
        {.name = FY_NATIVE_CLI_TEXT("next"), .has_arg = no_argument, .flag = nullptr, .val = 'f'},
        {.name = FY_NATIVE_CLI_TEXT("previous"), .has_arg = no_argument, .flag = nullptr, .val = 'r'},
        {.name = FY_NATIVE_CLI_TEXT("seek-forward"), .has_arg = required_argument, .flag = nullptr, .val = 'F'},
        {.name = FY_NATIVE_CLI_TEXT("seek-backward"), .has_arg = required_argument, .flag = nullptr, .val = 'R'},
        {.name = FY_NATIVE_CLI_TEXT("volume"), .has_arg = required_argument, .flag = nullptr, .val = Volume},
        {.name = FY_NATIVE_CLI_TEXT("volume-up"), .has_arg = no_argument, .flag = nullptr, .val = VolumeUp},
        {.name = FY_NATIVE_CLI_TEXT("volume-down"), .has_arg = no_argument, .flag = nullptr, .val = VolumeDown},
        {.name = FY_NATIVE_CLI_TEXT("mute"), .has_arg = no_argument, .flag = nullptr, .val = 'm'},
        {.name = FY_NATIVE_CLI_TEXT("repeat"), .has_arg = required_argument, .flag = nullptr, .val = Repeat},
        {.name = FY_NATIVE_CLI_TEXT("shuffle"), .has_arg = required_argument, .flag = nullptr, .val = Shuffle},
        {.name = nullptr, .has_arg = 0, .flag = nullptr, .val = 0},
    };

    static const auto help = u"%1: fooyin [%2] [%3]\n"
                             "\n"
                             "%4:\n"
                             "  -h, --help                 %5\n"
                             "  -v, --version              %6\n"
                             "\n"
                             "%7:\n"
                             "  -t, --play-pause           %8\n"
                             "  -p, --play                 %9\n"
                             "  -u, --pause                %10\n"
                             "  -s, --stop                 %11\n"
                             "  -f, --next                 %12\n"
                             "  -r, --previous             %13\n"
                             "  -F, --seek-forward <time>  %14\n"
                             "  -R, --seek-backward <time> %15\n"
                             "\n"
                             "%16:\n"
                             "      --volume <percent>     %17\n"
                             "      --volume-up            %18\n"
                             "      --volume-down          %19\n"
                             "  -m, --mute                 %20\n"
                             "\n"
                             "%21:\n"
                             "      --repeat <mode>         %22\n"
                             "      --shuffle <mode>        %23\n"
                             "\n"
                             "%24:\n"
                             "  urls                       %25\n"_s;

    const auto setPlayerAction = [this](const PlayerAction action) {
        if(m_playerAction != PlayerAction::None) {
            m_message = QObject::tr("Only one player option can be used at a time.");
            return false;
        }
        m_playerAction = action;
        return true;
    };

    const auto setVolumeAction = [this](const VolumeAction action) {
        if(m_volumeAction != VolumeAction::None) {
            m_message = QObject::tr("Only one volume option can be used at a time.");
            return false;
        }
        m_volumeAction = action;
        return true;
    };

    for(;;) {
        const int c = getopt_long(m_argc, m_argv, FY_NATIVE_CLI_TEXT(":hvxtpusfrmF:R:"), cmdOptions, nullptr);
        if(c == -1) {
            break;
        }

        switch(c) {
            case 'h': {
                m_message = QString{help}.arg(
                    QObject::tr("Usage"), QObject::tr("options"), QObject::tr("urls"), QObject::tr("Options"),
                    QObject::tr("Display help on command line options"), QObject::tr("Display version information"),
                    QObject::tr("Player options"), QObject::tr("Toggle playback"), QObject::tr("Start playback"),
                    QObject::tr("Pause playback"), QObject::tr("Stop playback"), QObject::tr("Skip to next track"),
                    QObject::tr("Skip to previous track"), QObject::tr("Seek forward (e.g. 5000, 10s, or 1:30)"),
                    QObject::tr("Seek backward (e.g. 5000, 10s, or 1:30)"), QObject::tr("Volume options"),
                    QObject::tr("Set volume from 0 to 100"), QObject::tr("Increase volume by the configured step"),
                    QObject::tr("Decrease volume by the configured step"), QObject::tr("Toggle mute"),
                    QObject::tr("Playback mode options"), QObject::tr("Set repeat: off, playlist, album, or track"),
                    QObject::tr("Set shuffle: off, tracks, albums, or random"), QObject::tr("Arguments"),
                    QObject::tr("Files, directories, or HTTP(S) URLs to open"));
                return ParseResult::ExitSuccess;
            }
            case 'v': {
                m_message = u"%1 %2"_s.arg(u"fooyin"_s, QLatin1String(VERSION));
                return ParseResult::ExitSuccess;
            }
            case 'x':
                m_skipSingle = true;
                break;
            case 't':
                if(!setPlayerAction(PlayerAction::PlayPause)) {
                    return ParseResult::ExitFailure;
                }
                break;
            case 'p':
                if(!setPlayerAction(PlayerAction::Play)) {
                    return ParseResult::ExitFailure;
                }
                break;
            case 'u':
                if(!setPlayerAction(PlayerAction::Pause)) {
                    return ParseResult::ExitFailure;
                }
                break;
            case 's':
                if(!setPlayerAction(PlayerAction::Stop)) {
                    return ParseResult::ExitFailure;
                }
                break;
            case 'f':
                if(!setPlayerAction(PlayerAction::Next)) {
                    return ParseResult::ExitFailure;
                }
                break;
            case 'r':
                if(!setPlayerAction(PlayerAction::Previous)) {
                    return ParseResult::ExitFailure;
                }
                break;
            case 'F':
                if(optarg) {
                    const QString value = decodeName(optarg);
                    const auto seekTime = parseSeekTime(value);
                    if(!seekTime) {
                        m_message = QObject::tr("Invalid seek time: %1").arg(value);
                        return ParseResult::ExitFailure;
                    }
                    if(!setPlayerAction(PlayerAction::SeekFwd)) {
                        return ParseResult::ExitFailure;
                    }
                    m_seekDelta = *seekTime;
                    break;
                }
                m_message = QObject::tr("Missing time for seek option.");
                return ParseResult::ExitFailure;
            case 'R':
                if(optarg) {
                    const QString value = decodeName(optarg);
                    const auto seekTime = parseSeekTime(value);
                    if(!seekTime) {
                        m_message = QObject::tr("Invalid seek time: %1").arg(value);
                        return ParseResult::ExitFailure;
                    }
                    if(!setPlayerAction(PlayerAction::SeekBack)) {
                        return ParseResult::ExitFailure;
                    }
                    m_seekDelta = *seekTime;
                    break;
                }
                m_message = QObject::tr("Missing time for seek option.");
                return ParseResult::ExitFailure;
            case Volume: {
                const QString value = decodeName(optarg);
                bool valid{false};
                const double percent = value.toDouble(&valid);
                if(!valid || !std::isfinite(percent) || percent < 0.0 || percent > 100.0) {
                    m_message = QObject::tr("Volume must be between 0 and 100: %1").arg(value);
                    return ParseResult::ExitFailure;
                }
                if(!setVolumeAction(VolumeAction::Set)) {
                    return ParseResult::ExitFailure;
                }
                m_volume = percent / 100.0;
                break;
            }
            case VolumeUp:
                if(!setVolumeAction(VolumeAction::Increase)) {
                    return ParseResult::ExitFailure;
                }
                break;
            case VolumeDown:
                if(!setVolumeAction(VolumeAction::Decrease)) {
                    return ParseResult::ExitFailure;
                }
                break;
            case 'm':
                if(!setVolumeAction(VolumeAction::ToggleMute)) {
                    return ParseResult::ExitFailure;
                }
                break;
            case Repeat: {
                if(m_repeatMode != RepeatMode::Unchanged) {
                    m_message = QObject::tr("The repeat option can only be specified once.");
                    return ParseResult::ExitFailure;
                }
                const QString value = decodeName(optarg);
                const auto mode     = parseRepeatMode(value);
                if(!mode) {
                    m_message = QObject::tr("Invalid repeat mode: %1").arg(value);
                    return ParseResult::ExitFailure;
                }
                m_repeatMode = *mode;
                break;
            }
            case Shuffle: {
                if(m_shuffleMode != ShuffleMode::Unchanged) {
                    m_message = QObject::tr("The shuffle option can only be specified once.");
                    return ParseResult::ExitFailure;
                }
                const QString value = decodeName(optarg);
                const auto mode     = parseShuffleMode(value);
                if(!mode) {
                    m_message = QObject::tr("Invalid shuffle mode: %1").arg(value);
                    return ParseResult::ExitFailure;
                }
                m_shuffleMode = *mode;
                break;
            }
            case ':':
                m_message = QObject::tr("Option requires an argument: %1").arg(decodeName(m_argv[optind - 1]));
                return ParseResult::ExitFailure;
            case '?':
                m_message = QObject::tr("Unknown option: %1").arg(decodeName(m_argv[optind - 1]));
                return ParseResult::ExitFailure;
            default:
                m_message = QObject::tr("Unable to parse command line options.");
                return ParseResult::ExitFailure;
        }
    }

    for(int i{optind}; i < m_argc; ++i) {
        const QString input = decodeName(m_argv[i]);
        const QUrl url{input, QUrl::StrictMode};
        const QString localPath = url.isLocalFile() ? url.toLocalFile() : input;
        const QFileInfo fileInfo{localPath};

        if(fileInfo.exists()) {
            m_files.append(QUrl::fromLocalFile(fileInfo.canonicalFilePath()));
        }
        else if(isRemoteUrl(url)) {
            m_files.append(url);
        }
        else {
            m_files.clear();
            m_message = QObject::tr("File or URL does not exist or is not supported: %1").arg(input);
            return ParseResult::ExitFailure;
        }
    }

    return ParseResult::Run;
}

#undef FY_NATIVE_CLI_TEXT

QString CommandLine::message() const
{
    return m_message;
}

std::optional<uint64_t> CommandLine::parseSeekTime(const QStringView value)
{
    if(value.isEmpty() || value.trimmed() != value) {
        return {};
    }

    if(value.contains(u':')) {
        const auto parts = value.split(u':');
        if(parts.size() < 2 || parts.size() > 3) {
            return {};
        }

        uint64_t seconds{0};
        for(int i{0}; i < parts.size(); ++i) {
            const auto part = parseUnsigned(parts.at(i));
            if(!part || (i > 0 && *part >= 60)) {
                return {};
            }
            if(seconds > (std::numeric_limits<uint64_t>::max() - *part) / 60) {
                return {};
            }
            seconds = (seconds * 60) + *part;
        }
        return multiply(seconds, 1000);
    }

    uint64_t multiplier{1};
    QStringView number = value;
    if(value.endsWith(u"ms")) {
        number = value.chopped(2);
    }
    else if(value.endsWith(u's')) {
        number     = value.chopped(1);
        multiplier = 1000;
    }
    else if(value.endsWith(u'm')) {
        number     = value.chopped(1);
        multiplier = 60ULL * 1000;
    }
    else if(value.endsWith(u'h')) {
        number     = value.chopped(1);
        multiplier = 60ULL * 60 * 1000;
    }

    const auto amount = parseUnsigned(number);
    return amount ? multiply(*amount, multiplier) : std::nullopt;
}

bool CommandLine::empty() const
{
    return m_files.empty() && !m_skipSingle && m_playerAction == PlayerAction::None
        && m_volumeAction == VolumeAction::None && m_repeatMode == RepeatMode::Unchanged
        && m_shuffleMode == ShuffleMode::Unchanged;
}

QList<QUrl> CommandLine::files() const
{
    return m_files;
}

uint64_t CommandLine::seekDelta() const
{
    return m_seekDelta;
}

bool CommandLine::skipSingleApp() const
{
    return m_skipSingle;
}

CommandLine::PlayerAction CommandLine::playerAction() const
{
    return m_playerAction;
}

CommandLine::VolumeAction CommandLine::volumeAction() const
{
    return m_volumeAction;
}

double CommandLine::volume() const
{
    return m_volume;
}

CommandLine::RepeatMode CommandLine::repeatMode() const
{
    return m_repeatMode;
}

CommandLine::ShuffleMode CommandLine::shuffleMode() const
{
    return m_shuffleMode;
}

QByteArray CommandLine::saveOptions() const
{
    QByteArray out;
    QDataStream stream(&out, QDataStream::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);

    stream << m_files;
    stream << m_skipSingle;
    stream << static_cast<quint8>(m_playerAction);
    stream << static_cast<quint64>(m_seekDelta);
    stream << static_cast<quint8>(m_volumeAction);
    stream << m_volume;
    stream << static_cast<quint8>(m_repeatMode);
    stream << static_cast<quint8>(m_shuffleMode);

    return out;
}

bool CommandLine::loadOptions(const QByteArray& options)
{
    QByteArray in{options};
    QDataStream stream(&in, QDataStream::ReadOnly);
    stream.setVersion(QDataStream::Qt_6_0);

    QList<QUrl> files;
    bool skipSingle{false};
    quint8 playerAction{0};
    quint64 seekDelta{0};
    quint8 volumeAction{0};
    double volume{0.0};
    quint8 repeatMode{0};
    quint8 shuffleMode{0};

    stream >> files;
    stream >> skipSingle;
    stream >> playerAction;
    stream >> seekDelta;
    stream >> volumeAction;
    stream >> volume;
    stream >> repeatMode;
    stream >> shuffleMode;

    if(stream.status() != QDataStream::Ok || !stream.atEnd()) {
        return false;
    }

    if(playerAction > static_cast<quint8>(PlayerAction::SeekBack)) {
        return false;
    }

    if(volumeAction > static_cast<quint8>(VolumeAction::ToggleMute) || !std::isfinite(volume) || volume < 0.0
       || volume > 1.0) {
        return false;
    }

    if(repeatMode > static_cast<quint8>(RepeatMode::Track) || shuffleMode > static_cast<quint8>(ShuffleMode::Random)) {
        return false;
    }

    m_files        = std::move(files);
    m_skipSingle   = skipSingle;
    m_playerAction = static_cast<PlayerAction>(playerAction);
    m_seekDelta    = static_cast<uint64_t>(seekDelta);
    m_volumeAction = static_cast<VolumeAction>(volumeAction);
    m_volume       = volume;
    m_repeatMode   = static_cast<RepeatMode>(repeatMode);
    m_shuffleMode  = static_cast<ShuffleMode>(shuffleMode);

    return true;
}
