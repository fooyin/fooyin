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

#pragma once

#include <core/engine/audioencoder.h>
#include <gui/widgets/extendabletableview.h>

namespace Fooyin {
struct EncoderProfileEntry
{
    AudioEncoderInfo info;
    QString storageId;
    bool builtIn{true};
    bool persisted{false};
};

class EncoderProfileTableModel : public ExtendableTableModel
{
    Q_OBJECT

public:
    EncoderProfileTableModel(QList<EncoderProfileEntry>* profiles, std::vector<AudioEncoderInfo>* encoders,
                             QObject* parent = nullptr);

    [[nodiscard]] static QString profileDescription(const EncoderProfile& profile);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;

    void addPendingRow() override;
    void removePendingRow() override;
    bool removeRows(int row, int count, const QModelIndex& parent = {}) override;

    void insertPendingProfile(const AudioEncoderInfo& initial);
    void commitPendingProfile(AudioEncoderInfo info);
    void resetProfiles();
    void refreshRow(int row);

Q_SIGNALS:
    void addProfileRequested();
    void profilesChanged();

private:
    QList<EncoderProfileEntry>* m_profiles;
    std::vector<AudioEncoderInfo>* m_encoders;
    int m_pendingRow;
};

} // namespace Fooyin
