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

#include "lyricssaver.h"

#include "lyrics.h"
#include "lyricsparser.h"
#include "settings/lyricssettings.h"
#include "sources/lyricsource.h"

#include <core/library/musiclibrary.h>
#include <core/track.h>
#include <utils/settings/settingsmanager.h>

#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QTimerEvent>

using namespace Qt::StringLiterals;

constexpr auto AutosaveTimer = 60000;

namespace {
struct DuplicateLine
{
    QString line;
    std::vector<uint64_t> timestamps;
};

template <typename Device, typename Format>
void toLrc(const Fooyin::Lyrics::Lyrics& lyrics, Device device, Format format,
           const Fooyin::Lyrics::LyricsSaver::SaveOptions& options)
{
    if(!lyrics.isValid()) {
        return;
    }

    using namespace Fooyin::Lyrics;

    const bool synced      = lyrics.type != Lyrics::Type::Unsynced;
    const bool syncedWords = lyrics.type == Lyrics::Type::SyncedWords;

    QTextStream stream{device};

    if(options == LyricsSaver::None) {
        stream << lyrics.data;
        return;
    }

    if(!synced) {
        for(const auto& line : lyrics.lines) {
            const QString words = line.joinedWords();
            stream << words.trimmed();
            stream << "\n";
        }
        return;
    }

    bool tagAdded{false};

    const auto addTag = [&stream, &tagAdded](const char* tag, const QString& value) {
        if(!value.isEmpty()) {
            stream << "[" << tag << ":" << value << "]" << "\n";
            tagAdded = true;
        }
    };

    if(options & LyricsSaver::SaveOption::Metadata) {
        addTag("ti", lyrics.metadata.title);
        addTag("ar", lyrics.metadata.artist);
        addTag("al", lyrics.metadata.album);
        addTag("au", lyrics.metadata.author);
        addTag("length", lyrics.metadata.length);
        addTag("by", lyrics.metadata.lrcAuthor);
        addTag("tool", lyrics.metadata.tool);
        addTag("ve", lyrics.metadata.version);
    }

    addTag("offset", lyrics.offset > 0 ? QString::number(lyrics.offset) : QString{});

    if(tagAdded) {
        stream << "\n";
    }

    if(!syncedWords && options & LyricsSaver::SaveOption::Collapse) {
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
                stream << u"[%1]"_s.arg(format(timestamp));
            }

            stream << duplicate.line;
            stream << "\n";
        }
        return;
    }

    for(const auto& line : lyrics.lines) {
        stream << u"[%1]"_s.arg(format(line.timestamp));
        if(syncedWords) {
            for(const auto& word : line.words) {
                stream << " " << u"<%1>"_s.arg(format(word.timestamp)) << " " << word.word;
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
    , m_hasPendingAutoSave{false}
{ }

void LyricsSaver::autoSaveLyrics(const Lyrics& lyrics, const Track& track)
{
    clearAutoSaveTimer();

    if(track.isInArchive()) {
        return;
    }

    const auto preferType = static_cast<SavePrefer>(m_settings->fileValue(Settings::SavePrefer, 0).toInt());
    if(preferType == SavePrefer::Unsynced && lyrics.type != Lyrics::Type::Unsynced) {
        return;
    }
    if(preferType == SavePrefer::Synced && lyrics.type == Lyrics::Type::Unsynced) {
        return;
    }

    const auto saveScheme = static_cast<SaveScheme>(m_settings->fileValue(Settings::SaveScheme, 0).toInt());

    switch(saveScheme) {
        case SaveScheme::Autosave:
            saveToConfiguredMethod(lyrics, track);
            break;
        case SaveScheme::AutosavePeriod:
            m_pendingAutoSaveLyrics = lyrics;
            m_pendingAutoSaveTrack  = track;
            m_hasPendingAutoSave    = true;
            m_autosaveTimer.start(std::min(AutosaveTimer, static_cast<int>(track.duration() / 3)), this);
            break;
        case SaveScheme::Manual:
            break;
    }
}

bool LyricsSaver::saveLyrics(const Lyrics& lyrics, const Track& track)
{
    clearAutoSaveTimer();

    if(track.isInArchive()) {
        return false;
    }

    return saveToConfiguredMethod(lyrics, track);
}

Lyrics LyricsSaver::savedLyrics(const Lyrics& lyrics, const Track& track)
{
    Lyrics savedLyrics{lyrics};
    savedLyrics.isLocal = true;

    const auto saveMethod = static_cast<SaveMethod>(
        m_settings->fileValue(Settings::SaveMethod, static_cast<int>(SaveMethod::Directory)).toInt());

    switch(saveMethod) {
        case SaveMethod::Tag:
            savedLyrics.source   = u"Metadata Tags"_s;
            savedLyrics.tag      = configuredLyricsTag(lyrics);
            savedLyrics.filepath = QString{};
            break;
        case SaveMethod::Directory:
            savedLyrics.source   = u"Local Files"_s;
            savedLyrics.tag      = QString{};
            savedLyrics.filepath = configuredLyricsFilepath(track);
            break;
    }

    return savedLyrics;
}

void LyricsSaver::writeLyricsToTags(const TrackList& tracks)
{
    if(tracks.empty()) {
        return;
    }

    m_library->writeTrackMetadata(tracks);
}

bool LyricsSaver::saveLyricsToFile(const Lyrics& lyrics, const Track& track)
{
    if(track.isInArchive()) {
        return false;
    }

    const QString filepath = configuredLyricsFilepath(track);

    const bool changedFile = !lyrics.filepath.isEmpty() && filepath != lyrics.filepath;

    const auto removeFile = [&track](const QString& file) {
        qCInfo(LYRICS) << "Removing file" << file << "for" << track.prettyFilepath();
        QFile{file}.moveToTrash();
    };

    if(lyrics.isEmpty()) {
        if(QFile::exists(filepath)) {
            QFile{filepath}.moveToTrash();
        }
        if(changedFile) {
            removeFile(lyrics.filepath);
        }
        return true;
    }

    QFile file{filepath};
    if(!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCInfo(LYRICS) << "Unable open file" << filepath << "to save lyrics";
        return false;
    }

    lyricsToLrc(lyrics, &file, static_cast<SaveOptions>(m_settings->fileValue(Settings::SaveOptions, 0).toInt()));
    file.close();

    if(changedFile) {
        removeFile(lyrics.filepath);
    }

    return true;
}

std::optional<Track> LyricsSaver::writeLyricsToTag(const Lyrics& lyrics, const Track& track)
{
    const auto updatedTrack = updateLyricsTag(lyrics, track);
    if(!updatedTrack) {
        return {};
    }

    m_library->writeTrackMetadata({*updatedTrack});
    return updatedTrack;
}

std::optional<Track> LyricsSaver::updateLyricsTag(const Lyrics& lyrics, const Track& track) const
{
    if(track.isInArchive()) {
        return {};
    }

    const QString tag     = configuredLyricsTag(lyrics);
    const bool changedTag = tag != lyrics.tag;

    if(tag.isEmpty()) {
        qCInfo(LYRICS) << "Unable to save lyrics to an empty tag";
        return {};
    }

    const QString lrc
        = lyricsToLrc(lyrics, static_cast<SaveOptions>(m_settings->fileValue(Settings::SaveOptions, 0).toInt()));

    Track updatedTrack{track};

    if(changedTag && !lyrics.tag.isEmpty()) {
        qCInfo(LYRICS) << "Removing original lyrics tag" << lyrics.tag << "from" << track.prettyFilepath();
        updatedTrack.removeExtraTag(lyrics.tag);
    }

    updatedTrack.replaceExtraTag(tag, lrc);
    return updatedTrack;
}

Track LyricsSaver::restoreLyricsTags(const Track& originalTrack, const Track& track) const
{
    Track restoredTrack{track};

    QStringList tags;
    tags.append(m_settings->fileValue(Settings::SaveSyncedTag, u"LYRICS"_s).toString());
    tags.append(m_settings->fileValue(Settings::SaveUnsyncedTag, u"UNSYNCED LYRICS"_s).toString());
    tags.removeAll(QString{});
    tags.removeDuplicates();

    for(const QString& tag : std::as_const(tags)) {
        if(originalTrack.hasExtraTag(tag)) {
            restoredTrack.replaceExtraTag(tag, originalTrack.extraTag(tag));
        }
        else {
            restoredTrack.removeExtraTag(tag);
        }
    }

    return restoredTrack;
}

QString LyricsSaver::lyricsToLrc(const Lyrics& lyrics, const SaveOptions& options)
{
    QString lrc;
    toLrc(lyrics, &lrc, formatTimestamp, options);
    return lrc;
}

void LyricsSaver::lyricsToLrc(const Lyrics& lyrics, QIODevice* device, const SaveOptions& options)
{
    toLrc(lyrics, device, formatTimestamp, options);
}

void LyricsSaver::timerEvent(QTimerEvent* event)
{
    if(event->timerId() != m_autosaveTimer.timerId()) {
        QObject::timerEvent(event);
        return;
    }

    if(!m_hasPendingAutoSave) {
        m_autosaveTimer.stop();
        return;
    }

    const Lyrics lyrics{m_pendingAutoSaveLyrics};
    const Track track{m_pendingAutoSaveTrack};
    clearAutoSaveTimer();

    saveToConfiguredMethod(lyrics, track);
}

void LyricsSaver::clearAutoSaveTimer()
{
    m_autosaveTimer.stop();
    m_pendingAutoSaveLyrics = {};
    m_pendingAutoSaveTrack  = {};
    m_hasPendingAutoSave    = false;
}

QString LyricsSaver::configuredLyricsFilepath(const Track& track)
{
    const QString dir      = m_settings->fileValue(Settings::SaveDir, u"%path%"_s).toString();
    const QString filename = m_settings->fileValue(Settings::SaveFilename, u"%filename%"_s).toString() + ".lrc"_L1;
    return QDir::cleanPath(m_parser.evaluate(dir + "/"_L1 + filename, track));
}

QString LyricsSaver::configuredLyricsTag(const Lyrics& lyrics) const
{
    QString tag = lyrics.type == Lyrics::Type::Unsynced
                    ? m_settings->fileValue(Settings::SaveUnsyncedTag, u"UNSYNCED LYRICS"_s).toString()
                    : m_settings->fileValue(Settings::SaveSyncedTag, u"LYRICS"_s).toString();

    if(tag.isEmpty()) {
        tag = lyrics.tag;
    }

    return tag;
}

bool LyricsSaver::saveToConfiguredMethod(const Lyrics& lyrics, const Track& track)
{
    const auto saveMethod = static_cast<SaveMethod>(
        m_settings->fileValue(Settings::SaveMethod, static_cast<int>(SaveMethod::Directory)).toInt());
    const auto conflictPolicy = static_cast<SaveConflictPolicy>(
        m_settings->fileValue(Settings::SaveConflict, static_cast<int>(SaveConflictPolicy::KeepOriginal)).toInt());
    const bool removeOriginal = (conflictPolicy == SaveConflictPolicy::RemoveOriginal);

    switch(saveMethod) {
        case SaveMethod::Tag: {
            const auto updatedTrack = writeLyricsToTag(lyrics, track);
            if(!updatedTrack.has_value()) {
                return false;
            }

            if(removeOriginal) {
                QStringList filesToRemove;
                filesToRemove.append(configuredLyricsFilepath(track));
                if(!lyrics.filepath.isEmpty()) {
                    filesToRemove.append(lyrics.filepath);
                }
                filesToRemove.removeAll(QString{});
                filesToRemove.removeDuplicates();

                for(const QString& filepath : std::as_const(filesToRemove)) {
                    if(QFile::exists(filepath)) {
                        qCInfo(LYRICS) << "Removing original lyrics file" << filepath << "for"
                                       << track.prettyFilepath();
                        QFile{filepath}.moveToTrash();
                    }
                }
            }

            Q_EMIT lyricsSaved(*updatedTrack, savedLyrics(lyrics, *updatedTrack));
            break;
        }
        case SaveMethod::Directory: {
            if(!saveLyricsToFile(lyrics, track)) {
                return false;
            }

            Track updatedTrack{track};
            bool tagsChanged = false;

            if(removeOriginal) {
                QStringList tagsToRemove;
                tagsToRemove.append(configuredLyricsTag(lyrics));
                if(!lyrics.tag.isEmpty()) {
                    tagsToRemove.append(lyrics.tag);
                }
                tagsToRemove.removeAll(QString{});
                tagsToRemove.removeDuplicates();

                for(const QString& tag : std::as_const(tagsToRemove)) {
                    if(updatedTrack.hasExtraTag(tag)) {
                        qCInfo(LYRICS) << "Removing original lyrics tag" << tag << "for" << track.prettyFilepath();
                        updatedTrack.removeExtraTag(tag);
                        tagsChanged = true;
                    }
                }
            }

            if(tagsChanged) {
                m_library->writeTrackMetadata({updatedTrack});
            }

            Q_EMIT lyricsSaved(updatedTrack, savedLyrics(lyrics, updatedTrack));
            break;
        }
    }

    return true;
}
} // namespace Fooyin::Lyrics
