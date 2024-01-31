/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include <utils/extendabletableview.h>
#include <core/track.h>
#include <utils/tablemodel.h>

namespace Fooyin {
class SettingsManager;

namespace TagEditor {
class TagEditorModel : public ExtendableTableModel
{
    Q_OBJECT

public:
    explicit TagEditorModel(SettingsManager* settings, QObject* parent = nullptr);
    ~TagEditorModel() override;

    void reset(const TrackList& tracks);
    void processQueue();

    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    [[nodiscard]] QModelIndex index(int row, int column, const QModelIndex& parent) const override;
    [[nodiscard]] int rowCount(const QModelIndex& parent) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;

    [[nodiscard]] bool removeRows(int row, int count,  const QModelIndex &parent) override;

    [[nodiscard]] static QString defaultFieldText();
    [[nodiscard]] bool tagsHaveChanged() const;

    void addPendingRow() override;
    void removePendingRow() override;

signals:
    void trackMetadataChanged(const TrackList& tracks);

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace TagEditor
} // namespace Fooyin
