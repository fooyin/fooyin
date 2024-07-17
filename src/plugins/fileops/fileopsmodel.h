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
#include "fileopsworker.h"

#include <QAbstractItemModel>
#include <QThread>

namespace Fooyin {
class MusicLibrary;

namespace FileOps {
struct FileOpsItem;

class FileOpsModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    FileOpsModel(MusicLibrary* library, TrackList tracks, SettingsManager* settings, QObject* parent = nullptr);
    ~FileOpsModel() override;

    void simulate(const FileOpPreset& preset);
    void run();
    void stop();

    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QModelIndex index(int row, int column, const QModelIndex& parent) const override;
    [[nodiscard]] QModelIndex parent(const QModelIndex& child) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;
    [[nodiscard]] int rowCount(const QModelIndex& parent) const override;

signals:
    void simulated();
    void finished();

private:
    void populate(const FileOperations& operations);
    void operationFinished(const FileOpsItem& operation);

    QThread m_workerThread;
    FileOpsWorker m_worker;
    FileOperations m_operations;
};
} // namespace FileOps
} // namespace Fooyin
