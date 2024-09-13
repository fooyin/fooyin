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

#include <core/track.h>
#include <utils/treestatusitem.h>

namespace Fooyin {
class ReplayGainItem : public TreeStatusItem<ReplayGainItem>
{
public:
    enum ItemType : uint16_t
    {
        Header = Qt::UserRole,
        Entry,
        TrackGain,
        TrackPeak,
        AlbumGain,
        AlbumPeak
    };

    enum Role : uint16_t
    {
        Type = Qt::UserRole + 5,
        Value
    };

    ReplayGainItem();
    ReplayGainItem(ItemType type, QString name, float value, const Track& track, ReplayGainItem* parent);
    ReplayGainItem(ItemType type, QString name, ReplayGainItem* parent);
    ReplayGainItem(ItemType type, QString name, const Track& track, ReplayGainItem* parent);

    bool operator<(const ReplayGainItem& other) const;

    [[nodiscard]] ItemType type() const;
    [[nodiscard]] QString name() const;
    [[nodiscard]] Track track() const;

    [[nodiscard]] float trackGain() const;
    [[nodiscard]] float trackPeak() const;
    [[nodiscard]] float albumGain() const;
    [[nodiscard]] float albumPeak() const;

    [[nodiscard]] bool isSummary() const;
    [[nodiscard]] bool isEditable() const;
    [[nodiscard]] bool multipleValues() const;

    bool setValue(float value);
    bool setTrackGain(float value);
    bool setTrackPeak(float value);
    bool setAlbumGain(float value);
    bool setAlbumPeak(float value);

    void setIsEditable(bool isEditable);
    void setMultipleValues(bool multiple);

    bool applyChanges();

private:
    template <typename T>
    bool setGainOrPeak(T& currentValue, float value, float trackValue, float invalidValue)
    {
        if(currentValue == value) {
            return false;
        }

        if(value == trackValue) {
            setStatus(None);
            if(std::exchange(currentValue, invalidValue) == invalidValue) {
                return false;
            }
        }
        else {
            currentValue     = value;
            m_multipleValues = false;
            setStatus(Changed);
        }

        return true;
    }

    ItemType m_type;
    QString m_name;
    Track m_track;
    bool m_summaryItem;
    bool m_isEditable;
    bool m_multipleValues;

    float m_trackGain;
    float m_trackPeak;
    float m_albumGain;
    float m_albumPeak;
};
} // namespace Fooyin
