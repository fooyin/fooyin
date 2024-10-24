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

#include "lyricssaver.h"

#include "lyrics.h"
#include "settings/lyricssettings.h"
#include "sources/lyricsource.h"

#include <core/library/musiclibrary.h>
#include <core/track.h>
#include <utils/settings/settingsmanager.h>

#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QTimer>

constexpr auto AutosaveTimer = 60000;

namespace {
QString formatTimestamp(uint64_t timestampMs)
{
    const uint64_t minutes    = timestampMs / 60000;
    const uint64_t seconds    = (timestampMs % 60000) / 1000;
    const uint64_t hundredths = (timestampMs % 1000) / 10;

    return QStringLiteral("%1:%2.%3")
        .arg(minutes, 2, 10, QLatin1Char{'0'})
        .arg(seconds, 2, 10, QLatin1Char{'0'})
        .arg(hundredths, 2, 10, QLatin1Char{'0'});
}

struct DuplicateLine
{
    QString line;
    std::vector<uint64_t> timestamps;
};

template <typename T>
void lyricsToLrc(const Fooyin::Lyrics::Lyrics& lyrics, T device, bool collapse)
{
    if(!lyrics.isValid()) {
        return;
    }

    const bool synced      = lyrics.type != Fooyin::Lyrics::Lyrics::Type::Unsynced;
    const bool syncedWords = lyrics.type == Fooyin::Lyrics::Lyrics::Type::SyncedWords;

    QTextStream stream{device};

    if(!synced) {
        for(const auto& line : lyrics.lines) {
            const QString words = line.joinedWords();
            stream << words.trimmed();
            stream << "\n";
        }
        return;
    }

    if(lyrics.offset > 0) {
        stream << "[offset:" << lyrics.offset << "]\n";
    }

    if(!syncedWords && collapse) {
        std::map<QString, std::vector<uint64_t>> duplicateLines;
        for(const auto& line : lyrics.lines) {
            duplicateLines[line.joinedWords()].push_back(line.timestamp);
        }

        std::map<uint64_t, DuplicateLine> sortedLines;
        for(const auto& [line, timestamps] : duplicateLines) {
            auto& duplicate = sortedLines[timestamps.front()];
            duplicate.line  = line;
            std::ranges::copy(timestamps, std::back_inserter(duplicate.timestamps));
        }

        for(const auto& [line, duplicate] : sortedLines) {
            for(const auto timestamp : duplicate.timestamps) {
                stream << QStringLiteral("[%1]").arg(formatTimestamp(timestamp));
            }

            stream << duplicate.line;
            stream << "\n";
        }
        return;
    }

    for(const auto& line : lyrics.lines) {
        stream << QStringLiteral("[%1]").arg(formatTimestamp(line.timestamp));
        if(syncedWords) {
            for(const auto& word : line.words) {
                stream << " " << QStringLiteral("<%1>").arg(formatTimestamp(word.timestamp)) << " " << word.word;
            }
        }
        else {
            const QString words = line.joinedWords();
            stream << words.trimmed();
        }

        stream << "\n";
    }
}
} // namespace

namespace Fooyin::Lyrics {
LyricsSaver::LyricsSaver(MusicLibrary* library, SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , m_library{library}
    , m_settings{settings}
    , m_autosaveTimer{nullptr}
{ }

void LyricsSaver::autoSaveLyrics(const Lyrics& lyrics, const Track& track)
{
    if(m_autosaveTimer) {
        m_autosaveTimer->stop();
        m_autosaveTimer->deleteLater();
    }

    if(!lyrics.isValid() || track.isInArchive()) {
        return;
    }

    const auto preferType = static_cast<SavePrefer>(m_settings->value<Settings::Lyrics::SavePrefer>());
    if(preferType == SavePrefer::Unsynced && lyrics.type != Lyrics::Type::Unsynced) {
        return;
    }
    if(preferType == SavePrefer::Synced && lyrics.type == Lyrics::Type::Unsynced) {
        return;
    }

    const auto saveToMethod = [this, lyrics, track]() {
        const auto saveMethod = static_cast<SaveMethod>(m_settings->value<Settings::Lyrics::SaveMethod>());
        switch(saveMethod) {
            case(SaveMethod::Tag):
                saveLyricsToTag(lyrics, track);
                break;
            case(SaveMethod::Directory):
                saveLyricsToFile(lyrics, track);
                break;
        }
    };

    const auto saveScheme = static_cast<SaveScheme>(m_settings->value<Settings::Lyrics::SaveScheme>());

    switch(saveScheme) {
        case(SaveScheme::Autosave):
            saveToMethod();
            break;
        case(SaveScheme::AutosavePeriod): {
            m_autosaveTimer = new QTimer(this);
            m_autosaveTimer->setSingleShot(true);
            QObject::connect(m_autosaveTimer, &QTimer::timeout, this, saveToMethod);
            m_autosaveTimer->start(std::min(AutosaveTimer, static_cast<int>(track.duration() / 3)));
        }
        case(SaveScheme::Manual):
            break;
    }
}

void LyricsSaver::saveLyrics(const Lyrics& lyrics, const Track& track)
{
    if(m_autosaveTimer) {
        m_autosaveTimer->stop();
        m_autosaveTimer->deleteLater();
    }

    if(!lyrics.isValid() || track.isInArchive()) {
        return;
    }

    const auto saveMethod = static_cast<SaveMethod>(m_settings->value<Settings::Lyrics::SaveMethod>());
    switch(saveMethod) {
        case(SaveMethod::Tag):
            saveLyricsToTag(lyrics, track);
            break;
        case(SaveMethod::Directory):
            saveLyricsToFile(lyrics, track);
            break;
    }
}

void LyricsSaver::saveLyricsToFile(const Lyrics& lyrics, const Track& track)
{
    if(!lyrics.isValid() || track.isInArchive()) {
        return;
    }

    const QString dir      = m_settings->value<Settings::Lyrics::SaveDir>();
    const QString filename = m_settings->value<Settings::Lyrics::SaveFilename>() + u".lrc";

    const QString filepath = QDir::cleanPath(m_parser.evaluate(dir + u"/" + filename, track));

    QFile file{filepath};
    if(!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCInfo(LYRICS) << "Unable open file" << filepath << "to save lyrics";
        return;
    }

    lyricsToLrc(lyrics, &file, m_settings->value<Settings::Lyrics::CollapseDuplicates>());
    file.close();
}

void LyricsSaver::saveLyricsToTag(const Lyrics& lyrics, const Track& track)
{
    if(!lyrics.isValid() || track.isInArchive()) {
        return;
    }

    const QString tag = lyrics.type == Lyrics::Type::Unsynced ? m_settings->value<Settings::Lyrics::SaveUnsyncedTag>()
                                                              : m_settings->value<Settings::Lyrics::SaveSyncedTag>();
    if(tag.isEmpty()) {
        qCInfo(LYRICS) << "Unable to save lyrics to an empty tag";
        return;
    }

    QString lrc;
    lyricsToLrc(lyrics, &lrc, m_settings->value<Settings::Lyrics::CollapseDuplicates>());

    if(lrc.isEmpty()) {
        return;
    }

    Track updatedTrack{track};
    updatedTrack.replaceExtraTag(tag, lrc);
    m_library->writeTrackMetadata({updatedTrack});
}
} // namespace Fooyin::Lyrics
