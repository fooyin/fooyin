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

#include "replaygainitem.h"

#include <core/constants.h>

namespace Fooyin {
ReplayGainItem::ReplayGainItem()
    : ReplayGainItem{Entry, {}, nullptr}
{ }

ReplayGainItem::ReplayGainItem(ItemType type, QString name, float value, const Track& track, ReplayGainItem* parent)
    : ReplayGainItem{type, std::move(name), track, parent}
{
    m_summaryItem = true;

    switch(type) {
        case(TrackGain):
            if(value != Constants::InvalidGain) {
                m_trackGain = value;
            }
            break;
        case(TrackPeak):
            if(value != Constants::InvalidPeak) {
                m_trackPeak = value;
            }
            break;
        case(AlbumGain):
            if(value != Constants::InvalidGain) {
                m_albumGain = value;
            }
            break;
        case(AlbumPeak):
            if(value != Constants::InvalidPeak) {
                m_albumPeak = value;
            }
            break;
        case(Header):
        case(Entry):
            break;
    }
}

ReplayGainItem::ReplayGainItem(ItemType type, QString name, ReplayGainItem* parent)
    : ReplayGainItem{type, std::move(name), {}, parent}
{ }

ReplayGainItem::ReplayGainItem(ItemType type, QString name, const Track& track, ReplayGainItem* parent)
    : TreeStatusItem{parent}
    , m_type{type}
    , m_name{std::move(name)}
    , m_track{track}
    , m_summaryItem{false}
    , m_isEditable{true}
    , m_multipleValues{false}
{ }

bool ReplayGainItem::operator<(const ReplayGainItem& /*other*/) const
{
    return false;
}

ReplayGainItem::ItemType ReplayGainItem::type() const
{
    return m_type;
}

QString ReplayGainItem::name() const
{
    return m_name;
}

Track ReplayGainItem::track() const
{
    return m_track;
}

float ReplayGainItem::trackGain() const
{
    return m_trackGain ? m_trackGain.value() : m_track.rgTrackGain();
}

float ReplayGainItem::trackPeak() const
{
    return m_trackPeak ? m_trackPeak.value() : m_track.rgTrackPeak();
}

float ReplayGainItem::albumGain() const
{
    return m_albumGain ? m_albumGain.value() : m_track.rgAlbumGain();
}

float ReplayGainItem::albumPeak() const
{
    return m_albumPeak ? m_albumPeak.value() : m_track.rgAlbumPeak();
}

bool ReplayGainItem::isSummary() const
{
    return m_summaryItem;
}

bool ReplayGainItem::isEditable() const
{
    return m_isEditable;
}

bool ReplayGainItem::multipleValues() const
{
    return m_multipleValues;
}

ReplayGainItem::SummaryFunc ReplayGainItem::summaryFunc() const
{
    return m_func;
}

bool ReplayGainItem::setTrackGain(float value)
{
    return setGainOrPeak(m_trackGain, value, m_track.rgTrackGain(), Constants::InvalidGain);
}

bool ReplayGainItem::setTrackPeak(float value)
{
    return setGainOrPeak(m_trackPeak, value, m_track.rgTrackPeak(), Constants::InvalidPeak);
}

bool ReplayGainItem::setAlbumGain(float value)
{
    return setGainOrPeak(m_albumGain, value, m_track.rgAlbumGain(), Constants::InvalidGain);
}

bool ReplayGainItem::setAlbumPeak(float value)
{
    return setGainOrPeak(m_albumPeak, value, m_track.rgAlbumPeak(), Constants::InvalidPeak);
}

void ReplayGainItem::setIsEditable(bool isEditable)
{
    m_isEditable = isEditable;
}

void ReplayGainItem::setMultipleValues(bool multiple)
{
    m_multipleValues = multiple;
}

void ReplayGainItem::setSummaryFunc(const SummaryFunc& func)
{
    m_func = func;
}

bool ReplayGainItem::applyChanges()
{
    if(m_isEditable && status() != Changed) {
        return false;
    }

    if(m_trackGain) {
        m_track.setRGTrackGain(m_trackGain.value());
        m_trackGain = {};
    }
    if(m_trackPeak) {
        m_track.setRGTrackPeak(m_trackPeak.value());
        m_trackPeak = {};
    }
    if(m_albumGain) {
        m_track.setRGAlbumGain(m_albumGain.value());
        m_albumGain = {};
    }
    if(m_albumPeak) {
        m_track.setRGAlbumPeak(m_albumPeak.value());
        m_albumPeak = {};
    }

    setStatus(None);
    return true;
}
} // namespace Fooyin
