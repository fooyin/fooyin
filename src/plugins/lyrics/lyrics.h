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
#include <QStringList>

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

    bool operator==(const ParsedWord& other) const noexcept
    {
        return std::tie(word, timestamp, duration) == std::tie(other.word, other.timestamp, other.duration);
    }

    bool operator!=(const ParsedWord& other) const noexcept
    {
        return !(*this == other);
    }

    [[nodiscard]] constexpr uint64_t isCurrent(uint64_t otherTimestamp) const
    {
        return otherTimestamp >= timestamp && otherTimestamp < endTimestamp();
    }

    [[nodiscard]] constexpr uint64_t endTimestamp() const
    {
        return timestamp + duration;
    }
};

struct ParsedLine
{
    uint64_t timestamp{0};
    uint64_t duration{0};
    std::vector<ParsedWord> words;

    bool operator==(const ParsedLine& other) const noexcept
    {
        return std::tie(timestamp, duration, words) == std::tie(other.timestamp, other.duration, other.words);
    }

    bool operator!=(const ParsedLine& other) const noexcept
    {
        return !(*this == other);
    }

    [[nodiscard]] constexpr uint64_t isCurrent(uint64_t otherTimestamp) const
    {
        return otherTimestamp >= timestamp && otherTimestamp < endTimestamp();
    }

    [[nodiscard]] constexpr uint64_t endTimestamp() const
    {
        return timestamp + duration;
    }

    [[nodiscard]] QString joinedWords() const
    {
        QString joined;
        for(const auto& word : words) {
            joined.append(word.word);
        }
        return joined;
    }
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
    QString source;
    bool isLocal{false};
    Metadata metadata;
    uint64_t offset{0};
    std::vector<ParsedLine> lines;

    [[nodiscard]] bool isValid() const noexcept
    {
        return type != Type::Unknown;
    }

    bool operator==(const Lyrics& other) const noexcept
    {
        return std::tie(type, offset, lines) == std::tie(other.type, other.offset, other.lines);
    }

    bool operator!=(const Lyrics& other) const noexcept
    {
        return !(*this == other);
    }
};
} // namespace Fooyin::Lyrics
