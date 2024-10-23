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

#include <core/scripting/scriptparser.h>

#include <QObject>

class QTimer;

namespace Fooyin {
class MusicLibrary;
class SettingsManager;
class Track;

namespace Lyrics {
struct Lyrics;

class LyricsSaver : public QObject
{
    Q_OBJECT

public:
    explicit LyricsSaver(MusicLibrary* library, SettingsManager* settings, QObject* parent = nullptr);

    void autoSaveLyrics(const Lyrics& lyrics, const Track& track);
    void saveLyrics(const Lyrics& lyrics, const Track& track);
    void saveLyricsToFile(const Lyrics& lyrics, const Track& track);
    void saveLyricsToTag(const Lyrics& lyrics, const Track& track);

private:
    MusicLibrary* m_library;
    SettingsManager* m_settings;

    ScriptParser m_parser;
    QTimer* m_autosaveTimer;
};
} // namespace Lyrics
} // namespace Fooyin
