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

#include "fileopsdefs.h"

#include <core/scripting/scriptparser.h>
#include <utils/worker.h>

#include <QDir>

#include <deque>
#include <set>

namespace Fooyin {
class MusicLibrary;
class SettingsManager;

namespace FileOps {
struct FileOpsItem
{
    Operation op;
    QString name;
    QString source;
    QString destination;

    [[nodiscard]] QString displayName() const
    {
        return !name.isEmpty() ? name : source;
    }

    [[nodiscard]] QString displayDestination() const
    {
        return op == Operation::Rename ? QFileInfo{destination}.fileName() : destination;
    }
};
using FileOperations = std::deque<FileOpsItem>;

class FileOpsWorker : public Worker
{
    Q_OBJECT

public:
    FileOpsWorker(MusicLibrary* library, TrackList tracks, SettingsManager* settings, QObject* parent = nullptr);

    void simulate(const FileOpPreset& preset);
    void run();

signals:
    void simulated(const Fooyin::FileOps::FileOperations& operations);
    void operationFinished(const Fooyin::FileOps::FileOpsItem& operation);

private:
    void simulateMove();
    void simulateCopy();
    void simulateRename();

    void renameFile(const FileOpsItem& item);
    static void copyFile(const FileOpsItem& item);

    void createDir(const QDir& dir);
    void removeDir(const QDir& dir);

    void reset();

    void handleEmptyDirs(const QDir& dir, const QString& filepath);
    void addEmptyDirs(const QDir& dir);

    MusicLibrary* m_library;
    SettingsManager* m_settings;
    ScriptParser m_scriptParser;
    TrackList m_tracks;
    std::unordered_multimap<QString, Track> m_trackPaths;
    FileOpPreset m_preset;
    std::deque<FileOpsItem> m_operations;

    bool m_isMonitoring;
    std::optional<QDir> m_currentDir;
    std::set<QString> m_tracksProcessed;
    std::set<QString> m_filesToMove;
    std::set<QString> m_dirsToCreate;
    std::set<QString> m_dirsToRemove;
    TrackList m_tracksToUpdate;
};
} // namespace FileOps
} // namespace Fooyin
