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

#include <core/engine/dsp/dspnode.h>

#include <QString>

#include <mutex>
#include <vector>

namespace Fooyin {
/*!
 * Thread-safe registry of available DSP node factories.
 *
 * Includes built-ins and plugin-provided entries.
 */
class FYCORE_EXPORT DspRegistry
{
public:
    DspRegistry();

    //! Register/replace a DSP factory by id.
    void registerDsp(const DspNode::Entry& entry);

    //! Snapshot of registered DSP entries.
    [[nodiscard]] std::vector<DspNode::Entry> entries() const;
    //! Instantiate DSP node by id, or nullptr if unavailable.
    [[nodiscard]] std::unique_ptr<DspNode> create(const QString& id) const;

private:
    void registerBuiltins();

    mutable std::mutex m_mutex;
    std::vector<DspNode::Entry> m_entries;
};
} // namespace Fooyin
