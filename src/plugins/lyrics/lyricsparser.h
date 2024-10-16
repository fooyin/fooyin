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

#include <QString>

namespace Fooyin::Lyrics {
struct Metadata
{
    QString title;
    QString artist;
    QString album;
    QString author;
    QString length;
    QString lrcAuthor;
    QString tool;
    QString version;
};

struct ParsedWord
{
    QString word;
    uint64_t timestamp{0};
    uint64_t duration{0};
};

struct ParsedLine
{
    uint64_t timestamp{0};
    uint64_t duration{0};
    std::vector<ParsedWord> words;
};

struct Lyrics
{
    enum class Type : uint8_t
    {
        Unknown = 0,
        Unsynced,
        Synced,
        SyncedWords
    };

    Type type{Type::Unknown};
    Metadata metadata;
    uint64_t offset{0};
    std::vector<ParsedLine> lines;
};

Lyrics parse(const QString& text);
Lyrics parse(const QByteArray& text);
} // namespace Fooyin::Lyrics
