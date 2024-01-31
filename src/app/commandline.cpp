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
{ }

bool CommandLine::parse()
{
    static const option cmdOptions[]
        = {{"help", no_argument, nullptr, 'h'}, {"version", no_argument, nullptr, 'v'}, {nullptr, 0, nullptr, 0}};

    static const QString help = "%1: fooyin [%2] [%3]\n"
                                "\n"
                                "%4:\n"
                                "  -h, --help      %5\n"
                                "  -v, --version   %6\n"
                                "\n"
                                "%7:\n"
                                "  urls            %8\n";

    for(;;) {
        const int c = getopt_long(m_argc, m_argv, "hv", cmdOptions, nullptr);
        if(c == -1) {
            break;
        }

        switch(c) {
            case('h'): {
                const auto helpText = QString{help}.arg(
                    QObject::tr("Usage"), QObject::tr("options"), QObject::tr("urls"), QObject::tr("Options"),
                    QObject::tr("Displays help on command line options"), QObject::tr("Displays version information"),
                    QObject::tr("Arguments"), QObject::tr("Files to open"));
                std::cout << helpText.toLocal8Bit().constData() << std::endl;
                return false;
            }
            case('v'): {
                const auto version = QString("%1 %2").arg("fooyin", VERSION);
                std::cout << version.toLocal8Bit().constData() << std::endl;
                std::exit(0);
            }
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
    return m_files.empty();
}

QList<QUrl> CommandLine::files() const
{
    return m_files;
}

QByteArray CommandLine::saveOptions() const
{
    QByteArray out;
    QDataStream stream(&out, QDataStream::WriteOnly);

    stream << m_files;

    return out;
}

void CommandLine::loadOptions(const QByteArray& options)
{
    QByteArray in{options};
    QDataStream stream(&in, QDataStream::ReadOnly);

    stream >> m_files;
}
