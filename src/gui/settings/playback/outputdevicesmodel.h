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

#include "output/outputprofilemanager.h"

#include <QAbstractTableModel>

#include <vector>

namespace Fooyin {
class DspPresetRegistry;

struct BitdepthOption
{
    QString text;
    SampleFormat bitDepth{SampleFormat::Unknown};
    bool dither{false};
};
using BitdepthOptions = std::vector<BitdepthOption>;

class OutputDevicesModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column
    {
        DeviceColumn = 0,
        DspColumn,
        BitsColumn,
        ColumnCount
    };

    enum Role
    {
        DeviceIdRole = Qt::UserRole + 1,
        DspPresetIdRole,
        BitDepthRole,
        DitherRole,
        BitdepthOptionsRole,
        BitdepthSelectionRole
    };

    explicit OutputDevicesModel(DspPresetRegistry* presetRegistry, QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;

    void setEntries(const std::vector<OutputProfileManager::DeviceEntry>& entries);
    [[nodiscard]] Engine::OutputDeviceProfiles profiles(const QString& output) const;

private:
    struct Row
    {
        QString device;
        QString description;
        bool enabled{true};
        SampleFormat bitDepth{SampleFormat::Unknown};
        bool dither{false};
        int dspPresetId{-1};
    };

    [[nodiscard]] QString presetText(int dspPresetId) const;
    static QString bitDepthText(SampleFormat bitDepth, bool dither);
    static BitdepthOptions buildBitdepthOptions();

    DspPresetRegistry* m_presetRegistry;
    std::vector<Row> m_rows;
};
} // namespace Fooyin

Q_DECLARE_METATYPE(Fooyin::BitdepthOptions)
Q_DECLARE_METATYPE(Fooyin::BitdepthOption)
