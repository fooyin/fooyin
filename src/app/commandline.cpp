/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include <QFile>
#include <QFileInfo>
#include <QObject>

#include <getopt.h>
#include <iostream>

CommandLine::CommandLine(int argc, char** argv)
    : m_argc{argc}
    , m_argv{argv}
    , m_skipSingle{false}
    , m_playerAction{PlayerAction::None}
{ }

bool CommandLine::parse()
{
    static constexpr option cmdOptions[]
        = {{"help", no_argument, nullptr, 'h'},        {"version", no_argument, nullptr, 'v'},
           {"skip-single", no_argument, nullptr, 'x'}, {"play-pause", no_argument, nullptr, 't'},
           {"play", no_argument, nullptr, 'p'},        {"pause", no_argument, nullptr, 'u'},
           {"stop", no_argument, nullptr, 's'},        {"next", no_argument, nullptr, 'f'},
           {"previous", no_argument, nullptr, 'r'},    {nullptr, 0, nullptr, 0}};

    static const auto help = QStringLiteral("%1: fooyin [%2] [%3]\n"
                                            "\n"
                                            "%4:\n"
                                            "  -h, --help      %5\n"
                                            "  -v, --version   %6\n"
                                            "\n"
                                            "%7:\n"
                                            "  -t, --play-pause  %8\n"
                                            "  -p, --play        %9\n"
                                            "  -u, --pause       %10\n"
                                            "  -s, --stop        %11\n"
                                            "  -f, --next        %12\n"
                                            "  -r, --previous    %13\n"
                                            "\n"
                                            "%14:\n"
                                            "  urls            %15\n");

    for(;;) {
        const int c = getopt_long(m_argc, m_argv, "hvxtpusfr", cmdOptions, nullptr);
        if(c == -1) {
            break;
        }

        switch(c) {
            case('h'): {
                const auto helpText = QString{help}.arg(
                    QObject::tr("Usage"), QObject::tr("options"), QObject::tr("urls"), QObject::tr("Options"),
                    QObject::tr("Display help on command line options"), QObject::tr("Display version information"),
                    QObject::tr("Player options"), QObject::tr("Toggle playback"), QObject::tr("Start playback"),
                    QObject::tr("Pause playback"), QObject::tr("Stop playback"), QObject::tr("Skip to next track"),
                    QObject::tr("Skip to previous track"), QObject::tr("Arguments"), QObject::tr("Files to open"));
                std::cout << helpText.toLocal8Bit().constData() << '\n';
                return false;
            }
            case('v'): {
                const auto version = QStringLiteral("%1 %2").arg(QStringLiteral("fooyin"), QLatin1String(VERSION));
                std::cout << version.toLocal8Bit().constData() << '\n';
                std::exit(0);
            }
            case('x'):
                m_skipSingle = true;
                break;
            case('t'):
                m_playerAction = PlayerAction::PlayPause;
                break;
            case('p'):
                m_playerAction = PlayerAction::Play;
                break;
            case('u'):
                m_playerAction = PlayerAction::Pause;
                break;
            case('s'):
                m_playerAction = PlayerAction::Stop;
                break;
            case('f'):
                m_playerAction = PlayerAction::Next;
                break;
            case('r'):
                m_playerAction = PlayerAction::Previous;
                break;
            default:
                return false;
        }
    }

    for(int i{optind}; i < m_argc; ++i) {
        const QFileInfo fileinfo{QFile::decodeName(m_argv[i])};
        if(fileinfo.exists()) {
            m_files.append(QUrl::fromLocalFile(fileinfo.canonicalFilePath()));
        }
    }

    return true;
}

bool CommandLine::empty() const
{
    return m_files.empty() && !m_skipSingle && m_playerAction == PlayerAction::None;
}

QList<QUrl> CommandLine::files() const
{
    return m_files;
}

bool CommandLine::skipSingleApp() const
{
    return m_skipSingle;
}

CommandLine::PlayerAction CommandLine::playerAction() const
{
    return m_playerAction;
}

QByteArray CommandLine::saveOptions() const
{
    QByteArray out;
    QDataStream stream(&out, QDataStream::WriteOnly);

    stream << m_files;
    stream << m_skipSingle;
    stream << static_cast<quint8>(m_playerAction);

    return out;
}

void CommandLine::loadOptions(const QByteArray& options)
{
    QByteArray in{options};
    QDataStream stream(&in, QDataStream::ReadOnly);

    stream >> m_files;
    stream >> m_skipSingle;

    quint8 playerAction{0};
    stream >> playerAction;
    m_playerAction = static_cast<PlayerAction>(playerAction);
}
