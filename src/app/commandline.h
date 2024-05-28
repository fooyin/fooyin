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

#pragma once

#include <QList>
#include <QUrl>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class CommandLine
{
public:
    enum class PlayerAction : uint8_t
    {
        None      = 0,
        PlayPause = 1,
        Play      = 2,
        Pause     = 3,
        Stop      = 4,
        Next      = 5,
        Previous  = 6,
    };

    explicit CommandLine(int argc = 0, char** argv = nullptr);

    bool parse();

    [[nodiscard]] bool empty() const;
    [[nodiscard]] QList<QUrl> files() const;
    [[nodiscard]] bool skipSingleApp() const;
    [[nodiscard]] PlayerAction playerAction() const;

    [[nodiscard]] QByteArray saveOptions() const;
    void loadOptions(const QByteArray& options);

private:
    int m_argc;
#ifdef Q_OS_WIN32
    LPWSTR* m_argv;
#else
    char** m_argv;
#endif
    QList<QUrl> m_files;
    bool m_skipSingle;
    PlayerAction m_playerAction;
};
