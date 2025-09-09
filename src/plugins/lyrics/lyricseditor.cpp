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

#include "lyricseditor.h"

#include "lyricsparser.h"
#include "lyricssaver.h"
#include "settings/lyricssettings.h"

#include <core/player/playercontroller.h>
#include <utils/settings/settingsmanager.h>

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextEdit>

using namespace Qt::StringLiterals;

constexpr auto TimestampRegex = R"(\[\d{2}:\d{2}\.\d{2,3}\])";

namespace Fooyin::Lyrics {
LyricsEditor::LyricsEditor(Lyrics lyrics, PlayerController* playerController, SettingsManager* settings,
                           QWidget* parent)
    : QDialog{parent}
    , m_playerController{playerController}
    , m_settings{settings}
    , m_lyrics{std::move(lyrics)}
    , m_playPause{new QPushButton(this)}
    , m_seek{new QPushButton(tr("Seek"), this)}
    , m_reset{new QPushButton(tr("Reset Changes"), this)}
    , m_insert{new QPushButton(tr("Insert/Update"), this)}
    , m_rewind{new QPushButton(tr("Rewind line (- 100ms)"), this)}
    , m_forward{new QPushButton(tr("Forward line (+ 100ms)"), this)}
    , m_remove{new QPushButton(tr("Remove"), this)}
    , m_removeAll{new QPushButton(tr("Remove All"), this)}
    , m_lyricsText{new QTextEdit(this)}
    , m_currentLineColour{palette().highlight().color()}
{
    m_currentLineColour.setAlpha(60);

    setWindowTitle(tr("Lyrics Editor"));

    auto* layout = new QGridLayout(this);

    auto* operationGroup  = new QGroupBox(tr("Operation"), this);
    auto* operationLayout = new QHBoxLayout(operationGroup);

    operationLayout->addWidget(m_playPause);
    operationLayout->addWidget(m_seek);
    operationLayout->addWidget(m_reset);

    auto* timestampsGroup  = new QGroupBox(tr("Timestamps"), this);
    auto* timestampsLayout = new QHBoxLayout(timestampsGroup);

    timestampsLayout->addWidget(m_insert);
    timestampsLayout->addWidget(m_rewind);
    timestampsLayout->addWidget(m_forward);
    timestampsLayout->addWidget(m_remove);
    timestampsLayout->addWidget(m_removeAll);

    auto* buttonBox
        = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel, this);
    QObject::connect(buttonBox->button(QDialogButtonBox::Apply), &QAbstractButton::clicked, this, &LyricsEditor::apply);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    layout->addWidget(operationGroup, 0, 0);
    layout->addWidget(timestampsGroup, 0, 1);
    layout->addWidget(m_lyricsText, 1, 0, 1, 2);
    layout->addWidget(buttonBox, 2, 0, 1, 2);
    layout->setRowStretch(1, 1);

    reset();
    updateButtons();

    QObject::connect(m_lyricsText, &QTextEdit::cursorPositionChanged, this, &LyricsEditor::highlightCurrentLine);
    QObject::connect(m_playPause, &QPushButton::clicked, m_playerController, &PlayerController::playPause);
    QObject::connect(m_seek, &QPushButton::clicked, this, &LyricsEditor::seek);
    QObject::connect(m_reset, &QPushButton::clicked, this, &LyricsEditor::reset);
    QObject::connect(m_insert, &QPushButton::clicked, this, &LyricsEditor::insertOrUpdateTimestamp);
    QObject::connect(m_rewind, &QPushButton::clicked, this, &LyricsEditor::adjustTimestamp);
    QObject::connect(m_forward, &QPushButton::clicked, this, &LyricsEditor::adjustTimestamp);
    QObject::connect(m_remove, &QPushButton::clicked, this, &LyricsEditor::removeTimestamp);
    QObject::connect(m_removeAll, &QPushButton::clicked, this, &LyricsEditor::removeAllTimestamps);

    QObject::connect(m_playerController, &PlayerController::positionChanged, this, &LyricsEditor::updateButtons);
    QObject::connect(m_playerController, &PlayerController::playStateChanged, this, &LyricsEditor::updateButtons);
}

void LyricsEditor::accept()
{
    apply();
    QDialog::accept();
}

void LyricsEditor::apply()
{
    const QString text = m_lyricsText->toPlainText();
    m_lyrics           = parse(text);
    m_lyrics.data      = text;
    emit lyricsEdited(m_lyrics);
}

QSize LyricsEditor::sizeHint() const
{
    return {700, 600};
}

void LyricsEditor::reset()
{
    m_lyricsText->setPlainText(m_lyrics.data);
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
        case(Player::PlayState::Playing):
            m_playPause->setText(tr("Pause") + time);
            break;
        case(Player::PlayState::Paused):
        case(Player::PlayState::Stopped):
            m_playPause->setText(tr("Play") + time);
            break;
    }
}

void LyricsEditor::highlightCurrentLine()
{
    QTextBlock block = m_lyricsText->document()->firstBlock();
    while(block.isValid()) {
        QTextCursor tempCursor{block};
        QTextBlockFormat format;
        format.setBackground(Qt::NoBrush);
        tempCursor.setBlockFormat(format);
        block = block.next();
    }

    QTextCursor currentCursor{m_lyricsText->textCursor().block()};
    QTextBlockFormat currentBlockFormat;
    currentBlockFormat.setBackground(m_currentLineColour);
    currentCursor.setBlockFormat(currentBlockFormat);
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
        if(sender() == m_rewind) {pos > 100 ? pos -= 100 : pos = 0;}
        else                     {pos < trackDuration - 100 ? pos += 100 : pos = trackDuration;}

        const QString newTimeStamp = u"[%1]"_s.arg(formatTimestamp(pos));
        currentLine.replace(regex, newTimeStamp);

        //to avoid skipping to next track in playlist during corrections near the end of a track.
        //alternative solutions are welcome.
        if(pos > trackDuration - 1000) {m_playerController->pause();}
        else                           {m_playerController->seek(pos);}
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
