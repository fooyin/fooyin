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

#include "lyrics.h"
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

    [[nodiscard]] Lyrics lyrics() const;

    void setDisplayString(const QString& string);
    void setLyrics(const Lyrics& lyrics);
    void setCurrentTime(uint64_t time);

    static QFont defaultFont();
    static QFont defaultLineFont();
    static QFont defaultWordLineFont();
    static QFont defaultWordFont();

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

signals:
    void currentLineChanged(int scrollValue);
    void linePressed(uint64_t timestamp);

protected:
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void timerEvent(QTimerEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void loadSettings();
    void recalculateArea();
    void highlightCurrentLine();
    void alignLineRects(std::vector<QRect>& lineRects, int line, int& wordX, int lineWidth);

    [[nodiscard]] bool syncedLyrics() const;
    [[nodiscard]] bool syncedWords() const;
    [[nodiscard]] QRect lineRect(int line) const;

    SettingsManager* m_settings;

    QString m_displayString;
    Lyrics m_lyrics;
    std::vector<std::vector<QRect>> m_lineRects;
    QBasicTimer m_resizeTimer;
    bool m_resizing;

    Qt::Alignment m_alignment;
    QMargins m_margins;
    int m_lineSpacing;

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
