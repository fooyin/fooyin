/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "libraryfileenumerator.h"

#include "libraryscanstate.h"
#include "libraryscanutils.h"

#include <QDir>

using namespace Qt::StringLiterals;

namespace Fooyin {
LibraryFileEnumerator::LibraryFileEnumerator(LibraryScanState* state, EnumeratedFileHandler handler)
    : m_state{state}
    , m_handler{std::move(handler)}
{ }

bool LibraryFileEnumerator::enumerateFiles(const QStringList& paths, const QStringList& trackExtensions,
                                           const QStringList& playlistExtensions)
{
    std::set<QString> visitedDirs;

    auto fileType
        = [&trackExtensions, &playlistExtensions](const QFileInfo& info) -> std::optional<EnumeratedFileType> {
        const QString suffix = info.suffix().toLower();

        if(suffix == "cue"_L1 && trackExtensions.contains(u"cue"_s)) {
            return EnumeratedFileType::Cue;
        }
        if(trackExtensions.contains(suffix)) {
            return EnumeratedFileType::Track;
        }
        if(playlistExtensions.contains(suffix)) {
            return EnumeratedFileType::Playlist;
        }

        return {};
    };

    auto emitFile = [this, &fileType](const QFileInfo& info) -> bool {
        if(!m_state->mayRun()) {
            return false;
        }

        if(const auto type = fileType(info)) {
            const QString filepath = normalisePath(info.absoluteFilePath());
            if(!m_state->rememberDiscoveredPath(filepath)) {
                return true;
            }

            m_state->fileDiscovered(filepath);
            return m_handler(info, type.value());
        }

        return true;
    };

    for(const auto& path : paths) {
        if(!m_state->mayRun()) {
            return false;
        }

        const QFileInfo info{path};
        if(!info.exists()) {
            continue;
        }

        const QString normalisedPath = normalisePath(info.absoluteFilePath());
        if(info.isDir()) {
            if(!processDirectory(normalisedPath, trackExtensions, playlistExtensions, visitedDirs)) {
                return false;
            }
            continue;
        }

        if(const auto type = fileType(info); type && type == EnumeratedFileType::Cue) {
            if(const auto cue = findMatchingCue(info)) {
                if(!emitFile(cue.value())) {
                    return false;
                }
            }
        }

        if(!emitFile(info)) {
            return false;
        }
    }

    return true;
}

bool LibraryFileEnumerator::processDirectory(const QString& path, const QStringList& trackExtensions,
                                             const QStringList& playlistExtensions, std::set<QString>& visitedDirs)
{
    if(!m_state->mayRun()) {
        return false;
    }

    const QFileInfo dirInfo{path};
    const QString canonicalPath = dirInfo.canonicalFilePath().isEmpty() ? path : dirInfo.canonicalFilePath();
    if(!visitedDirs.emplace(canonicalPath).second) {
        return true;
    }

    const QDir dir{path};
    const QFileInfoList entries
        = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase);
    QFileInfoList cueFiles;
    for(const auto& entry : entries) {
        if(entry.isFile() && entry.suffix().compare(u"cue"_s, Qt::CaseInsensitive) == 0
           && trackExtensions.contains(u"cue"_s)) {
            cueFiles.push_back(entry);
        }
    }

    auto emitTypedFile = [this](const QFileInfo& info, const EnumeratedFileType type) {
        const QString filepath = normalisePath(info.absoluteFilePath());
        if(!m_state->rememberDiscoveredPath(filepath)) {
            return true;
        }

        m_state->fileDiscovered(filepath);
        return m_handler(info, type);
    };

    auto findMatchingCueInDir = [&cueFiles](const QFileInfo& file) -> std::optional<QFileInfo> {
        for(const auto& cue : cueFiles) {
            if(cue.completeBaseName() == file.completeBaseName() || cue.fileName().contains(file.fileName())) {
                return cue;
            }
        }

        return {};
    };

    for(const auto& entry : entries) {
        if(entry.isDir()) {
            if(!processDirectory(normalisePath(entry.absoluteFilePath()), trackExtensions, playlistExtensions,
                                 visitedDirs)) {
                return false;
            }
            continue;
        }

        const QString suffix = entry.suffix().toLower();
        if(suffix == "cue"_L1 && trackExtensions.contains(u"cue"_s)) {
            if(!emitTypedFile(entry, EnumeratedFileType::Cue)) {
                return false;
            }
            continue;
        }

        if(trackExtensions.contains(suffix)) {
            if(const auto cue = findMatchingCueInDir(entry)) {
                if(!emitTypedFile(cue.value(), EnumeratedFileType::Cue)) {
                    return false;
                }
            }

            if(!emitTypedFile(entry, EnumeratedFileType::Track)) {
                return false;
            }
            continue;
        }

        if(playlistExtensions.contains(suffix)) {
            if(!emitTypedFile(entry, EnumeratedFileType::Playlist)) {
                return false;
            }
        }
    }

    return true;
}
} // namespace Fooyin
