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

class CommandLine
{
public:
    explicit CommandLine(int argc = 0, char** argv = nullptr);

    bool parse();

    [[nodiscard]] bool empty() const;
    [[nodiscard]] QList<QUrl> files() const;

    [[nodiscard]] QByteArray saveOptions() const;
    void loadOptions(const QByteArray& options);

private:
    int m_argc;
    char** m_argv;
    QList<QUrl> m_files;
};
