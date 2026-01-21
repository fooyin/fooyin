/*
 * Fooyin
 * Copyright © 2025, Luke Taylor <LukeT1@proton.me>
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

#include "lyrics.h"
#include "lyricscolours.h"

#include <gui/scripting/richtext.h>
#include <utils/settings/settingsmanager.h>

#include <QAbstractListModel>
#include <QMargins>

namespace Fooyin::Lyrics {
class LyricsModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role
    {
        RichTextRole = Qt::UserRole,
        TimestampRole,
        WordsRole,
        IsPaddingRole,
        MarginsRole,
        LineSpacingRole
    };

    explicit LyricsModel(SettingsManager* settings, QObject* parent = nullptr);

    void setLyrics(const Lyrics& lyrics);
    [[nodiscard]] Lyrics lyrics() const;

    void setMargins(const QMargins& margins);
    void setAlignment(Qt::Alignment alignment);
    void setLineSpacing(int spacing);

    void setCurrentTime(uint64_t time);
    [[nodiscard]] uint64_t currentTime() const;

    [[nodiscard]] int currentLineIndex() const;

    [[nodiscard]] int rowCount(const QModelIndex& parent) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;

private:
    void loadColours();
    void loadFonts();

    void updateCurrentLine();

    [[nodiscard]] RichText textForLine(const ParsedLine& line) const;
    [[nodiscard]] std::vector<Fooyin::Lyrics::ParsedLine>::const_iterator lineForTimestamp(uint64_t timestamp) const;
    [[nodiscard]] std::vector<Fooyin::Lyrics::ParsedWord>::const_iterator
    wordForTimestamp(const Fooyin::Lyrics::ParsedLine& line, uint64_t timestamp) const;

    SettingsManager* m_settings;

    Lyrics m_lyrics;
    QMargins m_margins;
    Qt::Alignment m_alignment;
    int m_lineSpacing;
    uint64_t m_currentTime;
    int m_currentLine;
    int m_currentWord;

    std::vector<RichText> m_text;

    Colours m_colours;
    QFont m_lineFont;
    QFont m_wordLineFont;
    QFont m_wordFont;
};
} // namespace Fooyin::Lyrics
