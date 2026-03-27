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
namespace {
float rawGainFromDisplay(const Track& track, float value)
{
    return value - track.opusHeaderGainDb();
}

float rawPeakFromDisplay(const Track& track, float value)
{
    return value * std::pow(10.0F, track.opusHeaderGainDb() / 20.0F);
}

float displayedTrackGain(const Track& track)
{
    return track.hasTrackGain() ? track.effectiveRGTrackGain() : Constants::InvalidGain;
}

float displayedTrackPeak(const Track& track)
{
    return track.hasTrackPeak() ? track.effectiveRGTrackPeak() : Constants::InvalidPeak;
}

float displayedAlbumGain(const Track& track)
{
    return track.hasAlbumGain() ? track.effectiveRGAlbumGain() : Constants::InvalidGain;
}

float displayedAlbumPeak(const Track& track)
{
    return track.hasAlbumPeak() ? track.effectiveRGAlbumPeak() : Constants::InvalidPeak;
}
} // namespace

ReplayGainItem::ReplayGainItem()
    : ReplayGainItem{Entry, {}, nullptr}
{ }

ReplayGainItem::ReplayGainItem(ItemType type, QString name, float value, const Track& track, ReplayGainItem* parent)
    : ReplayGainItem{type, std::move(name), track, parent}
{
    m_summaryItem = !track.isValid();

    switch(type) {
        case TrackGain:
            if(value != Constants::InvalidGain) {
                m_trackGain = value;
            }
            break;
        case TrackPeak:
            if(value != Constants::InvalidPeak) {
                m_trackPeak = value;
            }
            break;
        case AlbumGain:
            if(value != Constants::InvalidGain) {
                m_albumGain = value;
            }
            break;
        case AlbumPeak:
            if(value != Constants::InvalidPeak) {
                m_albumPeak = value;
            }
            break;
        case Header:
        case Entry:
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
    return m_trackGain ? m_trackGain.value() : displayedTrackGain(m_track);
}

float ReplayGainItem::trackPeak() const
{
    return m_trackPeak ? m_trackPeak.value() : displayedTrackPeak(m_track);
}

float ReplayGainItem::albumGain() const
{
    return m_albumGain ? m_albumGain.value() : displayedAlbumGain(m_track);
}

float ReplayGainItem::albumPeak() const
{
    return m_albumPeak ? m_albumPeak.value() : displayedAlbumPeak(m_track);
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

void ReplayGainItem::setTrack(const Track& track)
{
    m_track = track;
}

bool ReplayGainItem::setTrackGain(float value)
{
    return setGainOrPeak(m_trackGain, value, displayedTrackGain(m_track), Constants::InvalidGain, 2);
}

bool ReplayGainItem::setTrackPeak(float value)
{
    return setGainOrPeak(m_trackPeak, value, displayedTrackPeak(m_track), Constants::InvalidPeak, 6);
}

bool ReplayGainItem::setAlbumGain(float value)
{
    return setGainOrPeak(m_albumGain, value, displayedAlbumGain(m_track), Constants::InvalidGain, 2);
}

bool ReplayGainItem::setAlbumPeak(float value)
{
    return setGainOrPeak(m_albumPeak, value, displayedAlbumPeak(m_track), Constants::InvalidPeak, 6);
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
    if(status() != Changed) {
        return false;
    }

    if(m_trackGain) {
        m_track.setRGTrackGain(rawGainFromDisplay(m_track, m_trackGain.value()));
        m_trackGain = {};
    }
    if(m_trackPeak) {
        m_track.setRGTrackPeak(rawPeakFromDisplay(m_track, m_trackPeak.value()));
        m_trackPeak = {};
    }
    if(m_albumGain) {
        m_track.setRGAlbumGain(rawGainFromDisplay(m_track, m_albumGain.value()));
        m_albumGain = {};
    }
    if(m_albumPeak) {
        m_track.setRGAlbumPeak(rawPeakFromDisplay(m_track, m_albumPeak.value()));
        m_albumPeak = {};
    }

    setStatus(None);
    return true;
}

bool ReplayGainItem::sameEditableValue(float lhs, float rhs, float invalidValue, int precision)
{
    if(lhs == invalidValue || rhs == invalidValue) {
        return lhs == rhs;
    }

    return QString::number(lhs, 'f', precision) == QString::number(rhs, 'f', precision);
}
} // namespace Fooyin
