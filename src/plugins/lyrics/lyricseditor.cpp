/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include "lyricseditor.h"

#include "lyricsfinder.h"
#include "lyricsparser.h"
#include "lyricssaver.h"

#include <core/player/playercontroller.h>
#include <utils/utils.h>

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QTextEdit>
#include <QTextFormat>

using namespace Qt::StringLiterals;

constexpr auto TimestampRegex = R"(\[\d{2}:\d{2}\.\d{2,3}\])";

namespace Fooyin::Lyrics {
LyricsEditor::LyricsEditor(PlayerController* playerController, QWidget* parent)
    : QWidget{parent}
    , m_playerController{playerController}
{
    setupUi();
    setupConnections();
    setControlsEnabled(m_track.isValid() && m_track == m_playerController->currentTrack());
}

void LyricsEditor::setTrack(const Track& track)
{
    m_track = track;
    setControlsEnabled(m_track.isValid() && m_track == m_playerController->currentTrack());
}

void LyricsEditor::setLyrics(const Lyrics& lyrics)
{
    const bool textChanged = m_lyricsText->toPlainText() != lyrics.data;

    m_lyrics = lyrics;

    if(textChanged) {
        const QSignalBlocker blocker{m_lyricsText};
        m_lyricsText->setPlainText(m_lyrics.data);
    }
}

void LyricsEditor::setControlsEnabled(bool enabled)
{
    m_playPause->setEnabled(enabled);
    m_seek->setEnabled(enabled);
    m_insert->setEnabled(enabled);
    m_insertNext->setEnabled(enabled);
    m_rewind->setEnabled(enabled);
    m_forward->setEnabled(enabled);
}

QString LyricsEditor::text() const
{
    return m_lyricsText->toPlainText();
}

const Lyrics& LyricsEditor::currentLyrics() const
{
    return m_lyrics;
}

Lyrics LyricsEditor::editedLyrics() const
{
    Lyrics lyrics = lyricsFromText(m_lyricsText->toPlainText());

    lyrics.source   = m_lyrics.source;
    lyrics.isLocal  = m_lyrics.isLocal;
    lyrics.tag      = m_lyrics.tag;
    lyrics.filepath = m_lyrics.filepath;
    lyrics.metadata = m_lyrics.metadata;
    lyrics.offset   = m_lyrics.offset;

    return lyrics;
}

QSize LyricsEditor::sizeHint() const
{
    return Utils::proportionateSize(this, 0.25, 0.5);
}

Lyrics LyricsEditor::lyricsFromText(const QString& text) const
{
    Lyrics lyrics = parse(text);
    lyrics.data   = text;
    return lyrics;
}

void LyricsEditor::setupUi()
{
    setWindowTitle(tr("Lyrics Editor"));

    m_playPause  = new QPushButton(this);
    m_seek       = new QPushButton(tr("Seek"), this);
    m_reset      = new QPushButton(tr("Reset Changes"), this);
    m_insert     = new QPushButton(tr("Insert/Update"), this);
    m_insertNext = new QPushButton(tr("Update and Next Line"), this);
    m_rewind     = new QPushButton(tr("Rewind line (-100ms)"), this);
    m_forward    = new QPushButton(tr("Forward line (+100ms)"), this);
    m_remove     = new QPushButton(tr("Remove"), this);
    m_removeAll  = new QPushButton(tr("Remove All"), this);
    m_lyricsText = new QTextEdit(this);

    m_currentLineColour = palette().highlight().color();
    m_currentLineColour.setAlpha(60);

    auto* layout = new QGridLayout(this);

    auto* operationGroup  = new QGroupBox(tr("Operation"), this);
    auto* operationLayout = new QGridLayout(operationGroup);

    int row{0};
    operationLayout->addWidget(m_playPause, row, 0);
    operationLayout->addWidget(m_seek, row++, 1);
    operationLayout->addWidget(m_reset, row, 0, 1, 2);

    auto* timestampsGroup  = new QGroupBox(tr("Timestamps"), this);
    auto* timestampsLayout = new QGridLayout(timestampsGroup);

    row = 0;
    timestampsLayout->addWidget(m_insert, row, 0);
    timestampsLayout->addWidget(m_insertNext, row++, 1);
    timestampsLayout->addWidget(m_rewind, row, 0);
    timestampsLayout->addWidget(m_forward, row++, 1);
    timestampsLayout->addWidget(m_remove, row, 0);
    timestampsLayout->addWidget(m_removeAll, row, 1);

    row = 0;
    layout->addWidget(operationGroup, row, 0);
    layout->addWidget(timestampsGroup, row++, 1);
    layout->addWidget(m_lyricsText, row++, 0, 1, 2);
    layout->setRowStretch(1, 1);

    reset();
    updateButtons();
}

void LyricsEditor::setupConnections()
{
    QObject::connect(m_lyricsText, &QTextEdit::textChanged, this, &LyricsEditor::textEdited);
    QObject::connect(m_lyricsText, &QTextEdit::cursorPositionChanged, this, &LyricsEditor::highlightCurrentLine);
    QObject::connect(m_playPause, &QPushButton::clicked, m_playerController, &PlayerController::playPause);
    QObject::connect(m_seek, &QPushButton::clicked, this, &LyricsEditor::seek);
    QObject::connect(m_reset, &QPushButton::clicked, this, &LyricsEditor::reset);
    QObject::connect(m_insert, &QPushButton::clicked, this, &LyricsEditor::insertOrUpdateTimestamp);
    QObject::connect(m_insertNext, &QPushButton::clicked, this, &LyricsEditor::insertOrUpdateTimestampAndGotoNextLine);
    QObject::connect(m_rewind, &QPushButton::clicked, this, &LyricsEditor::adjustTimestamp);
    QObject::connect(m_forward, &QPushButton::clicked, this, &LyricsEditor::adjustTimestamp);
    QObject::connect(m_remove, &QPushButton::clicked, this, &LyricsEditor::removeTimestamp);
    QObject::connect(m_removeAll, &QPushButton::clicked, this, &LyricsEditor::removeAllTimestamps);

    QObject::connect(m_playerController, &PlayerController::positionChanged, this, &LyricsEditor::updateButtons);
    QObject::connect(m_playerController, &PlayerController::playStateChanged, this, &LyricsEditor::updateButtons);
}

void LyricsEditor::reset()
{
    m_lyricsText->setPlainText(m_lyrics.data);
    Q_EMIT resetClicked();
}

void LyricsEditor::seek()
{
    QTextCursor cursor = m_lyricsText->textCursor();
    cursor.select(QTextCursor::LineUnderCursor);
    const QString currentLine = cursor.selectedText();

    static const QRegularExpression regex{QLatin1String{TimestampRegex}};
    const QRegularExpressionMatch match = regex.match(currentLine);
    if(match.hasMatch()) {
        const uint64_t pos = timestampToMs(match.captured(0)) + m_lyrics.offset;
        m_playerController->seek(pos);
    }
}

void LyricsEditor::updateButtons()
{
    const auto time = u" [%1]"_s.arg(formatTimestamp(m_playerController->currentPosition()));

    switch(m_playerController->playState()) {
        case Player::PlayState::Playing:
            m_playPause->setText(tr("Pause") + time);
            break;
        case Player::PlayState::Paused:
        case Player::PlayState::Stopped:
            m_playPause->setText(tr("Play") + time);
            break;
    }
}

void LyricsEditor::highlightCurrentLine()
{
    QTextEdit::ExtraSelection selection;
    selection.format.setBackground(m_currentLineColour);
    selection.format.setProperty(QTextFormat::FullWidthSelection, true);
    selection.cursor = m_lyricsText->textCursor();
    selection.cursor.clearSelection();

    m_lyricsText->setExtraSelections({selection});
}

void LyricsEditor::insertOrUpdateTimestamp()
{
    QTextCursor cursor = m_lyricsText->textCursor();
    cursor.movePosition(QTextCursor::StartOfLine);
    cursor.select(QTextCursor::LineUnderCursor);
    QString currentLine = cursor.selectedText();

    QString newTimestamp = u"[%1]"_s.arg(formatTimestamp(m_playerController->currentPosition()));

    static const QRegularExpression regex{QLatin1String{TimestampRegex}};
    if(regex.match(currentLine).hasMatch()) {
        currentLine.replace(regex, newTimestamp);
    }
    else {
        currentLine = newTimestamp + currentLine;
    }

    cursor.insertText(currentLine);
}

void LyricsEditor::insertOrUpdateTimestampAndGotoNextLine()
{
    LyricsEditor::insertOrUpdateTimestamp();

    QTextCursor cursor = m_lyricsText->textCursor();
    cursor.movePosition(QTextCursor::StartOfLine);
    cursor.movePosition(QTextCursor::Down);
    cursor.movePosition(QTextCursor::StartOfLine);
    m_lyricsText->setTextCursor(cursor);
}

void LyricsEditor::adjustTimestamp()
{
    QTextCursor cursor = m_lyricsText->textCursor();
    cursor.select(QTextCursor::LineUnderCursor);
    QString currentLine = cursor.selectedText();

    static const QRegularExpression regex{QLatin1String{TimestampRegex}};
    const QRegularExpressionMatch match = regex.match(currentLine);

    const uint64_t trackDuration = m_playerController->currentTrack().duration();

    if(match.hasMatch()) {
        uint64_t pos = timestampToMs(match.captured(0)) + m_lyrics.offset;
        if(sender() == m_rewind) {
            pos > 100 ? pos -= 100 : pos = 0;
        }
        else {
            pos < trackDuration - 100 ? pos += 100 : pos = trackDuration;
        }

        const QString newTimeStamp = u"[%1]"_s.arg(formatTimestamp(pos));
        currentLine.replace(regex, newTimeStamp);

        // to avoid skipping to next track in playlist during corrections near the end of a track.
        // alternative solutions are welcome.
        if(pos > trackDuration - 1000) {
            m_playerController->pause();
        }
        else {
            m_playerController->seek(pos);
        }
    }
    cursor.insertText(currentLine);
}

void LyricsEditor::removeTimestamp()
{
    QTextCursor cursor = m_lyricsText->textCursor();
    cursor.select(QTextCursor::LineUnderCursor);
    QString currentLine = cursor.selectedText();

    static const QRegularExpression regex{QLatin1String{TimestampRegex}};
    currentLine.remove(regex);

    cursor.insertText(currentLine);
}

void LyricsEditor::removeAllTimestamps()
{
    QString currentText = m_lyricsText->toPlainText();

    static const QRegularExpression regex{QLatin1String{TimestampRegex}};
    currentText.remove(regex);

    m_lyricsText->setPlainText(currentText);
}
} // namespace Fooyin::Lyrics
