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

#include <core/engine/audiooutput.h>
#include <core/engine/fadingdefs.h>
#include <core/engine/levelframe.h>
#include <core/track.h>
#include <utils/datastream.h>

#include <QObject>

namespace Fooyin::Engine {
Q_NAMESPACE

//! High-level transport state.
enum class PlaybackState : uint8_t
{
    Stopped = 0,
    Playing,
    Paused,
    Error
};

//! Track lifecycle state reported by the engine.
enum class TrackStatus : uint8_t
{
    NoTrack = 0,
    Loading,
    Loaded,
    Buffered,
    End,
    Invalid,
    Unreadable
};

//! Track-state notification payload including generation for stale-event filtering.
struct TrackStatusContext
{
    TrackStatus status{TrackStatus::NoTrack};
    Track track;
    uint64_t generation{0};
};

//! Callback payload emitted shortly before the active track reaches end.
struct AboutToFinishContext
{
    Track track;
    uint64_t generation{0};
};

//! Result returned from asynchronous next-track prepare requests.
struct NextTrackPrepareRequest
{
    uint64_t requestId{0};
    bool readyNow{false};
};

enum RGProcess : uint8_t
{
    NoProcessing    = 0,
    ApplyGain       = 1 << 0,
    PreventClipping = 1 << 1,
};
Q_DECLARE_FLAGS(RGProcessing, RGProcess)
Q_FLAG_NS(RGProcessing)

//! Analysis frame types produced by the analysis bus.
enum AnalysisDataType : uint8_t
{
    NoAnalysisData = 0,
    LevelFrameData = 1 << 0,
};
Q_DECLARE_FLAGS(AnalysisDataTypes, AnalysisDataType)
Q_FLAG_NS(AnalysisDataTypes)

//! Serializable DSP node descriptor used in user-configured chains.
struct DspDefinition
{
    QString id;
    QString name;
    bool hasSettings{false};
    QByteArray settings;

    bool operator==(const DspDefinition& other) const noexcept
    {
        return std::tie(id, settings) == std::tie(other.id, other.settings);
    }
};

using DspChain = std::vector<DspDefinition>;

struct DspChains
{
    DspChain perTrackChain;
    DspChain masterChain;

    bool operator==(const DspChains&) const = default;

    [[nodiscard]] bool isEmpty() const
    {
        return perTrackChain.empty() && masterChain.empty();
    }

    void clear()
    {
        perTrackChain.clear();
        masterChain.clear();
    }
};

inline QDataStream& operator<<(QDataStream& stream, const DspDefinition& entry)
{
    stream << entry.id;
    stream << entry.name;
    stream << entry.hasSettings;
    stream << entry.settings;

    return stream;
}

inline QDataStream& operator>>(QDataStream& stream, DspDefinition& entry)
{
    stream >> entry.id;
    stream >> entry.name;
    stream >> entry.hasSettings;
    stream >> entry.settings;

    return stream;
}

inline QDataStream& operator<<(QDataStream& stream, const DspChains& entry)
{
    DataStream::writeContainer(stream, entry.perTrackChain);
    DataStream::writeContainer(stream, entry.masterChain);

    return stream;
}

inline QDataStream& operator>>(QDataStream& stream, DspChains& entry)
{
    DataStream::readContainer(stream, entry.perTrackChain);
    DataStream::readContainer(stream, entry.masterChain);

    return stream;
}

} // namespace Fooyin::Engine

Q_DECLARE_OPERATORS_FOR_FLAGS(Fooyin::Engine::RGProcessing)
Q_DECLARE_OPERATORS_FOR_FLAGS(Fooyin::Engine::AnalysisDataTypes)
Q_DECLARE_METATYPE(Fooyin::Engine::TrackStatusContext)
Q_DECLARE_METATYPE(Fooyin::Engine::AboutToFinishContext)
Q_DECLARE_METATYPE(Fooyin::Engine::AnalysisDataTypes)
Q_DECLARE_METATYPE(Fooyin::LevelFrame)
