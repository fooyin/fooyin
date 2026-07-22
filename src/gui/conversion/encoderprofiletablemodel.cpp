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

#include "encoderprofiletablemodel.h"

#include <QUuid>

#include <algorithm>
#include <utility>

using namespace Qt::StringLiterals;

namespace Fooyin {
EncoderProfileTableModel::EncoderProfileTableModel(QList<EncoderProfileEntry>* profiles,
                                                   std::vector<AudioEncoderInfo>* encoders, QObject* parent)
    : ExtendableTableModel{parent}
    , m_profiles{profiles}
    , m_encoders{encoders}
    , m_pendingRow{-1}
{ }

QString EncoderProfileTableModel::profileDescription(const EncoderProfile& profile)
{
    switch(profile.mode) {
        case EncoderMode::VariableBitrate:
            return tr("VBR");
        case EncoderMode::ConstrainedVariableBitrate:
            return tr("CVBR");
        case EncoderMode::ConstantQuality:
            return tr("VBR quality %1").arg(profile.quality, 0, 'f', 1);
        case EncoderMode::AverageBitrate:
            return tr("ABR");
        case EncoderMode::ConstantBitrate:
            return tr("CBR");
        case EncoderMode::LosslessCompression:
            return profile.compressionLevel >= 0 ? tr("level %1").arg(profile.compressionLevel) : QString{};
        case EncoderMode::Default:
            return {};
    }
    return {};
}

int EncoderProfileTableModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_profiles->size());
}

int EncoderProfileTableModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : 3;
}

QVariant EncoderProfileTableModel::data(const QModelIndex& index, int role) const
{
    if(!index.isValid() || index.row() >= static_cast<int>(m_profiles->size()) || role != Qt::DisplayRole) {
        return {};
    }

    const AudioEncoderInfo& info = m_profiles->at(index.row()).info;

    switch(index.column()) {
        case 0:
            return info.name;
        case 1: {
            switch(info.profile.mode) {
                case EncoderMode::VariableBitrate:
                case EncoderMode::ConstrainedVariableBitrate:
                case EncoderMode::AverageBitrate:
                case EncoderMode::ConstantBitrate:
                case EncoderMode::Default:
                    return info.profile.bitrateKbps > 0 ? tr("%1 kbps").arg(info.profile.bitrateKbps) : QString{};
                case EncoderMode::ConstantQuality: {
                    const int estimate = info.estimateBitrateKbps ? info.estimateBitrateKbps(info.profile) : 0;
                    return estimate > 0 ? tr("~%1 kbps").arg(estimate) : QString{};
                }
                case EncoderMode::LosslessCompression:
                    return {};
            }
            return {};
        }
        case 2: {
            const QString description = profileDescription(info.profile);
            return description.isEmpty() ? info.description : description;
        }
        default:
            return {};
    }
}

QVariant EncoderProfileTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }
    switch(section) {
        case 0:
            return tr("Name");
        case 1:
            return tr("Bitrate");
        case 2:
            return tr("Settings");
        default:
            return {};
    }
}

Qt::ItemFlags EncoderProfileTableModel::flags(const QModelIndex& index) const
{
    return index.isValid() ? Qt::ItemIsEnabled | Qt::ItemIsSelectable : Qt::NoItemFlags;
}

void EncoderProfileTableModel::addPendingRow()
{
    Q_EMIT addProfileRequested();
}

void EncoderProfileTableModel::removePendingRow()
{
    if(m_pendingRow < 0 || m_pendingRow >= static_cast<int>(m_profiles->size())) {
        return;
    }

    beginRemoveRows({}, m_pendingRow, m_pendingRow);
    m_profiles->erase(m_profiles->begin() + m_pendingRow);
    m_pendingRow = -1;
    endRemoveRows();
    Q_EMIT profilesChanged();
}

bool EncoderProfileTableModel::removeRows(int row, int count, const QModelIndex& parent)
{
    if(parent.isValid() || count != 1 || row < 0 || row >= static_cast<int>(m_profiles->size())) {
        return false;
    }

    auto& entry = (*m_profiles)[row];
    if(entry.builtIn) {
        if(!entry.persisted) {
            return false;
        }

        const auto runtime = std::ranges::find(*m_encoders, entry.info.id, &AudioEncoderInfo::id);
        if(runtime == m_encoders->end()) {
            return false;
        }

        entry.info      = *runtime;
        entry.storageId = u"builtin:%1"_s.arg(entry.info.id);
        entry.persisted = false;
        Q_EMIT dataChanged(index(row, 0), index(row, columnCount() - 1));
    }
    else {
        beginRemoveRows({}, row, row);
        m_profiles->erase(m_profiles->begin() + row);
        endRemoveRows();
    }

    Q_EMIT profilesChanged();
    return true;
}

void EncoderProfileTableModel::insertPendingProfile(const AudioEncoderInfo& initial)
{
    if(m_pendingRow >= 0) {
        return;
    }

    m_pendingRow = static_cast<int>(m_profiles->size());
    beginInsertRows({}, m_pendingRow, m_pendingRow);
    m_profiles->emplace_back(initial, QUuid::createUuid().toString(QUuid::WithoutBraces), false, false);
    endInsertRows();
}

void EncoderProfileTableModel::commitPendingProfile(AudioEncoderInfo info)
{
    if(m_pendingRow < 0 || m_pendingRow >= static_cast<int>(m_profiles->size())) {
        return;
    }

    auto& entry = (*m_profiles)[m_pendingRow];
    entry.info  = std::move(info);
    if(const QString description = profileDescription(entry.info.profile); !description.isEmpty()) {
        entry.info.description = description;
    }

    entry.persisted = true;
    const int row   = std::exchange(m_pendingRow, -1);
    Q_EMIT dataChanged(index(row, 0), index(row, columnCount() - 1));
    Q_EMIT profilesChanged();
}

void EncoderProfileTableModel::resetProfiles()
{
    beginResetModel();
    endResetModel();
}

void EncoderProfileTableModel::refreshRow(int row)
{
    Q_EMIT dataChanged(index(row, 0), index(row, columnCount() - 1));
}
} // namespace Fooyin
