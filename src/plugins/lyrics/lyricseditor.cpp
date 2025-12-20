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

#include "lyricsfinder.h"
#include "lyricsparser.h"
#include "lyricssaver.h"
#include "settings/lyricssettings.h"

#include <core/player/playercontroller.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

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
LyricsEditor::LyricsEditor(const Track& track, std::shared_ptr<NetworkAccessManager> networkAccess,
                           LyricsSaver* lyricsSaver, PlayerController* playerController, SettingsManager* settings,
                           QWidget* parent)
    : PropertiesTabWidget{parent}
    , m_track{track}
    , m_lyricsSaver{lyricsSaver}
    , m_playerController{playerController}
    , m_settings{settings}
    , m_networkAccess{std::move(networkAccess)}
    , m_lyricsFinder{new LyricsFinder(m_networkAccess, m_settings, this)}
{
    setupUi();
    setupConnections();
    updateTrack(m_track);
}

LyricsEditor::LyricsEditor(Lyrics lyrics, PlayerController* playerController, SettingsManager* settings,
                           QWidget* parent)
    : PropertiesTabWidget{parent}
    , m_lyricsSaver{nullptr}
    , m_playerController{playerController}
    , m_settings{settings}
    , m_lyricsFinder{nullptr}
    , m_lyrics{std::move(lyrics)}
{
    setupUi();
    setupConnections();
}

void LyricsEditor::updateTrack(const Track& track)
{
    m_track = track;
    m_lyricsFinder->findLocalLyrics(m_track);
}

QString LyricsEditor::name() const
{
    return tr("Lyrics Editor");
}

QString LyricsEditor::layoutName() const
{
    return u"LyricsEditor"_s;
}

void LyricsEditor::apply()
{
    const QString text = m_lyricsText->toPlainText();
    m_lyrics           = parse(text);
    m_lyrics.data      = text;
    emit lyricsEdited(m_lyrics);

    if(m_lyricsSaver && m_track.isValid()) {
        m_lyricsSaver->saveLyrics(m_lyrics, m_track);
    }
}

QSize LyricsEditor::sizeHint() const
{
    return Utils::proportionateSize(this, 0.25, 0.5);
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

    if(m_lyricsSaver && m_track.isValid() && m_track != m_playerController->currentTrack()) {
        m_playPause->setDisabled(true);
        m_seek->setDisabled(true);
        m_insert->setDisabled(true);
        m_insertNext->setDisabled(true);
        m_rewind->setDisabled(true);
        m_forward->setDisabled(true);
    }
}

void LyricsEditor::setupConnections()
{
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

    if(m_lyricsFinder) {
        QObject::connect(m_playerController, &PlayerController::currentTrackUpdated, this, &LyricsEditor::updateTrack);
        QObject::connect(m_lyricsFinder, &LyricsFinder::lyricsFound, this, [this](const Lyrics& lyrics) {
            m_lyrics = lyrics;
            reset();
        });
    }
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
void LyricsEditor::insertOrUpdateTimestampAndGotoNextLine()
{
	LyricsEditor::adjustTimestamp();
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

LyricsEditorDialog::LyricsEditorDialog(Lyrics lyrics, PlayerController* playerController, SettingsManager* settings,
                                       QWidget* parent)
    : QDialog{parent}
    , m_editor{new LyricsEditor(std::move(lyrics), playerController, settings, this)}
{
    setWindowTitle(tr("Lyrics Editor"));

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_editor);

    auto* buttonBox
        = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel, this);

    QObject::connect(buttonBox->button(QDialogButtonBox::Apply), &QAbstractButton::clicked, m_editor,
                     &PropertiesTabWidget::apply);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    layout->addWidget(buttonBox);
}

LyricsEditor* LyricsEditorDialog::editor() const
{
    return m_editor;
}

void LyricsEditorDialog::saveState()
{
    FyStateSettings stateSettings;
    Utils::saveState(this, stateSettings);
}

void LyricsEditorDialog::restoreState()
{
    const FyStateSettings stateSettings;
    Utils::restoreState(this, stateSettings);
}

void LyricsEditorDialog::accept()
{
    m_editor->apply();
    QDialog::accept();
}

QSize LyricsEditorDialog::sizeHint() const
{
    return m_editor->sizeHint();
}
} // namespace Fooyin::Lyrics
