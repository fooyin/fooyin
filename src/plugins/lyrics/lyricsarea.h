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

#include "lyricsparser.h"

#include "lyricscolours.h"

#include <QBasicTimer>
#include <QWidget>

namespace Fooyin {
class SettingsManager;

namespace Lyrics {
class LyricsArea : public QWidget
{
    Q_OBJECT

public:
    explicit LyricsArea(SettingsManager* settings, QWidget* parent = nullptr);

    void setLyrics(const Lyrics& lyrics);
    void setCurrentTime(uint64_t time);

    static QFont defaultLineFont();
    static QFont defaultWordLineFont();
    static QFont defaultWordFont();

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

signals:
    int currentLineChanged(int scrollValue);

protected:
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void timerEvent(QTimerEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void loadSettings();
    void recalculateArea(int startingLine = 0);
    void highlightCurrentLine();
    [[nodiscard]] QRect lineRect(int line) const;
    [[nodiscard]] std::vector<ParsedLine>::const_iterator lineForTimestamp(uint64_t timestamp) const;
    [[nodiscard]] std::vector<ParsedWord>::const_iterator wordForTimestamp(const ParsedLine& line,
                                                                           uint64_t timestamp) const;
    void alignLineRects(std::vector<QRect>& lineRects, int line, int& wordX, int lineWidth);

    SettingsManager* m_settings;

    Lyrics m_lyrics;
    std::vector<std::vector<QRect>> m_lineRects;
    QBasicTimer m_resizeTimer;
    bool m_resizing;

    Qt::Alignment m_alignment;
    int m_verticalMargin;
    int m_horizontalMargin;
    int m_lineHeight;
    int m_totalHeight;

    uint64_t m_currentTime;
    int m_currentLine;
    int m_currentWord;

    Colours m_colours;
    QFont m_lineFont;
    QFont m_wordLineFont;
    QFont m_wordFont;
};
} // namespace Lyrics
} // namespace Fooyin
