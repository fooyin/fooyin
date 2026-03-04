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
    NoProcessing = 0,
    //! Apply replaygain loudness adjustment.
    ApplyGain = 1 << 0,
    //! Apply limiter/clamp stage to avoid clipping after gain.
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
    bool enabled{true};
    uint64_t instanceId{0};
    QByteArray settings;

    bool operator==(const DspDefinition& other) const noexcept
    {
        return std::tie(id, enabled, instanceId, settings)
            == std::tie(other.id, other.enabled, other.instanceId, other.settings);
    }
};

using DspChain = std::vector<DspDefinition>;

//! User-configured DSP chain set grouped by processing stage.
struct DspChains
{
    //! DSPs applied before the master mix stage (per active track stream).
    DspChain perTrackChain;
    //! DSPs applied after mix on the master output path.
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
    stream << entry.enabled;
    stream << static_cast<quint64>(entry.instanceId);
    stream << entry.settings;

    return stream;
}

inline QDataStream& operator>>(QDataStream& stream, DspDefinition& entry)
{
    stream >> entry.id;
    stream >> entry.name;
    stream >> entry.hasSettings;
    stream >> entry.enabled;

    quint64 instanceId{0};
    stream >> instanceId;
    entry.instanceId = instanceId;

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

enum class DspChainScope : uint8_t
{
    PerTrack = 0,
    Master
};

//! UI track-switch anchor policy during crossfade overlap.
enum class CrossfadeSwitchPolicy : uint8_t
{
    OverlapStart = 0,
    Boundary,
};

//! Patch payload for live settings updates to a DSP instance.
struct LiveDspSettingsUpdate
{
    //! Target chain where the DSP instance is located.
    DspChainScope scope{DspChainScope::Master};
    //! Stable runtime instance identifier.
    uint64_t instanceId{0};
    //! Serialised settings payload passed to `DspNode::loadSettings`.
    QByteArray settings;
    //! Monotonic caller revision for stale-update filtering.
    uint64_t revision{0};
};

} // namespace Fooyin::Engine

Q_DECLARE_OPERATORS_FOR_FLAGS(Fooyin::Engine::RGProcessing)
Q_DECLARE_OPERATORS_FOR_FLAGS(Fooyin::Engine::AnalysisDataTypes)
Q_DECLARE_METATYPE(Fooyin::Engine::TrackStatusContext)
Q_DECLARE_METATYPE(Fooyin::Engine::AboutToFinishContext)
Q_DECLARE_METATYPE(Fooyin::Engine::AnalysisDataTypes)
Q_DECLARE_METATYPE(Fooyin::LevelFrame)
Q_DECLARE_METATYPE(Fooyin::Engine::LiveDspSettingsUpdate)
