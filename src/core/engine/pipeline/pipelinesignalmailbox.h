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

#include "fycore_export.h"

#include <atomic>
#include <cstdint>

class QObject;

namespace Fooyin {
/*!
 * Lock-free signal mailbox from audio thread to engine thread.
 *
 * The audio thread marks bit flags and posts at most one wake event until the
 * engine thread drains the pending mask.
 */
class FYCORE_EXPORT PipelineSignalMailbox
{
public:
    PipelineSignalMailbox();

    //! Configure wake-event target object/type for engine-thread notifications.
    void setWakeTarget(QObject* target, int wakeEventType);
    //! Atomically fetch-and-clear pending signal bits.
    [[nodiscard]] uint32_t drain();
    //! Clear specific bits from pending mask.
    void clear(uint32_t signalMask);
    //! Set specific bits and schedule wake event if needed.
    void mark(uint32_t signalMask);
    //! Clear pending state and posted-wake latch.
    void reset();
    //! Default wake event type used when caller does not provide one.
    [[nodiscard]] static int signalWakeEventType();

private:
    void postWakeIfNeeded();

    std::atomic<uint32_t> m_pendingSignalBits;
    std::atomic<bool> m_signalWakePosted;
    std::atomic<QObject*> m_signalWakeTarget;
    std::atomic<int> m_signalWakeEventType;
};
} // namespace Fooyin
