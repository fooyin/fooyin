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

constexpr auto InvalidGain = -1000;
constexpr auto InvalidPeak = -1;

namespace Fooyin {
ReplayGainItem::ReplayGainItem()
    : ReplayGainItem{Entry, {}, nullptr}
{ }

ReplayGainItem::ReplayGainItem(ItemType type, QString name, ReplayGainItem* parent)
    : ReplayGainItem{type, name, {}, parent}
{ }

ReplayGainItem::ReplayGainItem(ItemType type, QString name, Track track, ReplayGainItem* parent)
    : TreeStatusItem{parent}
    , m_type{type}
    , m_name{std::move(name)}
    , m_track{std::move(track)}
    , m_trackGain{InvalidGain}
    , m_trackPeak{InvalidPeak}
    , m_albumGain{InvalidGain}
    , m_albumPeak{InvalidPeak}
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
    return m_trackGain == InvalidGain ? m_track.rgTrackGain() : m_trackGain;
}

float ReplayGainItem::trackPeak() const
{
    return m_trackPeak == InvalidPeak ? m_track.rgTrackPeak() : m_trackPeak;
}

float ReplayGainItem::albumGain() const
{
    return m_albumGain == InvalidGain ? m_track.rgAlbumGain() : m_albumGain;
}

float ReplayGainItem::albumPeak() const
{
    return m_albumPeak == InvalidPeak ? m_track.rgAlbumPeak() : m_albumPeak;
}

bool ReplayGainItem::setTrackGain(float value)
{
    if(m_trackGain == value) {
        return false;
    }

    if(m_trackGain == value) {
        return false;
    }

    if(value == m_track.rgTrackGain()) {
        setStatus(None);
        if(std::exchange(m_trackGain, InvalidGain) == InvalidGain) {
            return false;
        }
    }
    else {
        m_trackGain = value;
        setStatus(Changed);
    }

    return true;
}

bool ReplayGainItem::setTrackPeak(float value)
{
    if(m_trackPeak == value) {
        return false;
    }

    if(value == m_track.rgTrackPeak()) {
        setStatus(None);
        if(std::exchange(m_trackPeak, InvalidPeak) == InvalidPeak) {
            return false;
        }
    }
    else {
        m_trackPeak = value;
        setStatus(Changed);
    }

    return true;
}

bool ReplayGainItem::setAlbumGain(float value)
{
    if(m_albumGain == value) {
        return false;
    }

    if(value == m_track.rgAlbumGain()) {
        setStatus(None);
        if(std::exchange(m_albumGain, InvalidGain) == InvalidGain) {
            return false;
        }
    }
    else {
        m_albumGain = value;
        setStatus(Changed);
    }

    return true;
}

bool ReplayGainItem::setAlbumPeak(float value)
{
    if(m_albumPeak == value) {
        return false;
    }

    if(value == m_track.rgAlbumPeak()) {
        setStatus(None);
        if(std::exchange(m_albumPeak, InvalidPeak) == InvalidPeak) {
            return false;
        }
    }
    else {
        m_albumPeak = value;
        setStatus(Changed);
    }

    return true;
}

bool ReplayGainItem::applyChanges()
{
    if(status() != Changed) {
        return false;
    }

    if(m_trackGain != InvalidGain) {
        m_track.setRGTrackGain(m_trackGain);
    }
    if(m_trackPeak != InvalidPeak) {
        m_track.setRGTrackPeak(m_trackPeak);
    }
    if(m_albumGain != InvalidGain) {
        m_track.setRGAlbumGain(m_albumGain);
    }
    if(m_albumPeak != InvalidPeak) {
        m_track.setRGAlbumPeak(m_albumPeak);
    }

    setStatus(None);
    return true;
}
} // namespace Fooyin
