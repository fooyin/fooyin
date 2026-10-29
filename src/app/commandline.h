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

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringView>
#include <QUrl>

#include <optional>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class CommandLine
{
public:
    enum class ParseResult : uint8_t
    {
        Run,
        ExitSuccess,
        ExitFailure,
    };

    enum class PlayerAction : uint8_t
    {
        None      = 0,
        PlayPause = 1,
        Play      = 2,
        Pause     = 3,
        Stop      = 4,
        Next      = 5,
        Previous  = 6,
        SeekFwd   = 7,
        SeekBack  = 8,
    };

    enum class VolumeAction : uint8_t
    {
        None = 0,
        Set,
        Increase,
        Decrease,
        ToggleMute,
    };

    enum class RepeatMode : uint8_t
    {
        Unchanged = 0,
        Off,
        Playlist,
        Album,
        Track,
    };

    enum class ShuffleMode : uint8_t
    {
        Unchanged = 0,
        Off,
        Tracks,
        Albums,
        Random,
    };

    explicit CommandLine(int argc = 0, char** argv = nullptr);

    ParseResult parse();

    [[nodiscard]] QString message() const;
    [[nodiscard]] static std::optional<uint64_t> parseSeekTime(QStringView value);

    [[nodiscard]] bool empty() const;
    [[nodiscard]] QList<QUrl> files() const;
    [[nodiscard]] uint64_t seekDelta() const;
    [[nodiscard]] bool skipSingleApp() const;
    [[nodiscard]] PlayerAction playerAction() const;
    [[nodiscard]] VolumeAction volumeAction() const;
    [[nodiscard]] double volume() const;
    [[nodiscard]] RepeatMode repeatMode() const;
    [[nodiscard]] ShuffleMode shuffleMode() const;

    [[nodiscard]] QByteArray saveOptions() const;
    bool loadOptions(const QByteArray& options);

private:
    int m_argc;
#ifdef Q_OS_WIN
    LPWSTR* m_argv;
#else
    char** m_argv;
#endif
    QList<QUrl> m_files;
    QString m_message;
    uint64_t m_seekDelta;
    double m_volume;
    bool m_skipSingle;
    PlayerAction m_playerAction;
    VolumeAction m_volumeAction;
    RepeatMode m_repeatMode;
    ShuffleMode m_shuffleMode;
};
