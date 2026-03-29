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

#include "outputdevicesmodel.h"

#include "dsp/dsppresetregistry.h"

#include <core/engine/audioformat.h>

#include <QCoreApplication>

using namespace Qt::StringLiterals;

namespace Fooyin {
OutputDevicesModel::OutputDevicesModel(DspPresetRegistry* presetRegistry, QObject* parent)
    : QAbstractTableModel{parent}
    , m_presetRegistry{presetRegistry}
{ }

int OutputDevicesModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

int OutputDevicesModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant OutputDevicesModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }

    switch(section) {
        case DeviceColumn:
            //: Audio output device
            return tr("Device");
        case DspColumn:
            //: DSP chain set for an audio output device
            return tr("DSP");
        case BitsColumn:
            return tr("Bitdepth");
        default:
            return {};
    }
}

QVariant OutputDevicesModel::data(const QModelIndex& index, int role) const
{
    if(!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_rows.size())) {
        return {};
    }

    const auto& row = m_rows.at(static_cast<size_t>(index.row()));

    if(role == Qt::TextAlignmentRole) {
        switch(index.column()) {
            case DeviceColumn:
                return {Qt::AlignVCenter | Qt::AlignLeft};
            case DspColumn:
            case BitsColumn:
                return Qt::AlignCenter;
            default:
                break;
        }
    }

    switch(index.column()) {
        case DeviceColumn:
            if(role == Qt::DisplayRole) {
                return row.description;
            }
            if(role == Qt::CheckStateRole) {
                return row.enabled ? Qt::Checked : Qt::Unchecked;
            }
            if(role == DeviceIdRole) {
                return row.device;
            }
            break;
        case DspColumn:
            if(role == Qt::DisplayRole) {
                return presetText(row.dspPresetId);
            }
            if(role == Qt::EditRole || role == DspPresetIdRole) {
                return row.dspPresetId;
            }
            break;
        case BitsColumn:
            if(role == Qt::DisplayRole) {
                return bitDepthText(row.bitDepth, row.dither);
            }
            if(role == BitdepthSelectionRole) {
                return QVariant::fromValue(BitdepthOption{
                    .text     = bitDepthText(row.bitDepth, row.dither),
                    .bitDepth = row.bitDepth,
                    .dither   = row.dither,
                });
            }
            if(role == BitDepthRole) {
                return static_cast<int>(row.bitDepth);
            }
            if(role == DitherRole) {
                return row.dither;
            }
            if(role == BitdepthOptionsRole) {
                return QVariant::fromValue(buildBitdepthOptions());
            }
            break;
        default:
            break;
    }

    return {};
}

bool OutputDevicesModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_rows.size())) {
        return false;
    }

    auto& row = m_rows.at(static_cast<size_t>(index.row()));
    switch(index.column()) {
        case DeviceColumn:
            if(role == Qt::CheckStateRole) {
                const bool enabled = value.toInt() == Qt::Checked;
                if(row.enabled == enabled) {
                    return false;
                }
                row.enabled = enabled;
                emit dataChanged(index, index, {Qt::CheckStateRole});
                return true;
            }
            break;
        case DspColumn:
            if(role == Qt::EditRole || role == DspPresetIdRole) {
                const int dspPresetId = value.toInt();
                if(row.dspPresetId == dspPresetId) {
                    return false;
                }
                row.dspPresetId = dspPresetId;
                emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole, DspPresetIdRole});
                return true;
            }
            break;
        case BitsColumn:
            if(role == BitdepthSelectionRole) {
                const auto option = value.value<BitdepthOption>();
                if(row.bitDepth == option.bitDepth && row.dither == option.dither) {
                    return false;
                }

                row.bitDepth = option.bitDepth;
                row.dither   = row.bitDepth == SampleFormat::S16 && option.dither;
                emit dataChanged(index, index, {Qt::DisplayRole, BitdepthSelectionRole, BitDepthRole, DitherRole});
                return true;
            }
            if(role == BitDepthRole) {
                const auto bitDepth = static_cast<SampleFormat>(value.toInt());
                const bool changed  = row.bitDepth != bitDepth;
                row.bitDepth        = bitDepth;
                if(row.bitDepth != SampleFormat::S16) {
                    row.dither = false;
                }
                if(changed) {
                    emit dataChanged(index, index, {Qt::DisplayRole, BitDepthRole, DitherRole});
                }
                return changed;
            }
            if(role == DitherRole) {
                const bool dither = row.bitDepth == SampleFormat::S16 && value.toBool();
                if(row.dither == dither) {
                    return false;
                }
                row.dither = dither;
                emit dataChanged(index, index, {Qt::DisplayRole, DitherRole});
                return true;
            }
            break;
        default:
            break;
    }

    return false;
}

Qt::ItemFlags OutputDevicesModel::flags(const QModelIndex& index) const
{
    if(!index.isValid()) {
        return Qt::NoItemFlags;
    }

    auto itemFlags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    switch(index.column()) {
        case DeviceColumn:
            return itemFlags | Qt::ItemIsUserCheckable;
        case DspColumn:
        case BitsColumn:
            return itemFlags | Qt::ItemIsEditable;
        default:
            return itemFlags;
    }
}

void OutputDevicesModel::setEntries(const std::vector<OutputProfileManager::DeviceEntry>& entries)
{
    beginResetModel();
    m_rows.clear();
    m_rows.reserve(entries.size());

    for(const auto& entry : entries) {
        m_rows.push_back({
            .device      = entry.device,
            .description = entry.description,
            .enabled     = entry.enabled,
            .bitDepth    = entry.bitDepth,
            .dither      = entry.dither,
            .dspPresetId = entry.dspPresetId,
        });
    }

    endResetModel();
}

Engine::OutputDeviceProfiles OutputDevicesModel::profiles(const QString& output) const
{
    Engine::OutputDeviceProfiles profiles;
    profiles.reserve(m_rows.size());

    for(const auto& row : m_rows) {
        profiles.push_back({
            .output      = output,
            .device      = row.device,
            .enabled     = row.enabled,
            .bitDepth    = row.bitDepth,
            .dither      = row.bitDepth == SampleFormat::S16 && row.dither,
            .dspPresetId = row.dspPresetId,
        });
    }

    return profiles;
}
QString OutputDevicesModel::presetText(int dspPresetId) const
{
    if(dspPresetId < 0) {
        //: No DSP chain has been set for this output device
        return tr("<not set>");
    }

    for(const auto& preset : m_presetRegistry->items()) {
        if(preset.id == dspPresetId) {
            return preset.name;
        }
    }

    //: The DSP chain set for this output device can no longer be found
    return tr("<missing>");
}

QString OutputDevicesModel::bitDepthText(SampleFormat bitDepth, bool dither)
{
    switch(bitDepth) {
        case SampleFormat::S16:
            return dither ? tr("16-bit (dithered)") : tr("16-bit");
        case SampleFormat::S24In32:
            return tr("24-bit");
        case SampleFormat::S32:
            return tr("32-bit");
        case SampleFormat::F32:
            return tr("32-bit float");
        case SampleFormat::Unknown:
        default:
            return tr("Automatic");
    }
}

BitdepthOptions OutputDevicesModel::buildBitdepthOptions()
{
    return {
        {.text = tr("Automatic"), .bitDepth = SampleFormat::Unknown},
        {.text = tr("16-bit"), .bitDepth = SampleFormat::S16},
        {.text = tr("16-bit (dithered)"), .bitDepth = SampleFormat::S16, .dither = true},
        {.text = tr("24-bit"), .bitDepth = SampleFormat::S24In32},
        {.text = tr("32-bit"), .bitDepth = SampleFormat::S32},
        {.text = tr("32-bit float"), .bitDepth = SampleFormat::F32},
    };
}
} // namespace Fooyin
