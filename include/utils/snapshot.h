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

#include <utils/compatutils.h>

namespace Fooyin {
template <typename T>
class Snapshot
{
public:
    struct Data
    {
        uint64_t epoch{0};
        T value{};
    };

    Snapshot()
        : m_data{std::make_shared<const Data>()}
    { }

    [[nodiscard]] std::shared_ptr<const Data> load() const noexcept
    {
        return m_data.load(std::memory_order_acquire);
    }

    template <typename Fn>
    void update(Fn&& updater)
    {
        auto current = load();
        auto next    = std::make_shared<Data>(*current);

        ++next->epoch;

        std::invoke(std::forward<decltype(updater)>(updater), next->value);
        m_data.store(std::shared_ptr<const Data>{std::move(next)}, std::memory_order_release);
    }

private:
    AtomicSharedPtr<const Data> m_data;
};
} // namespace Fooyin
