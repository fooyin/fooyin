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

#include <cassert>
#include <type_traits>
#include <utility>

namespace Fooyin {
template <typename Function>
class ScopeGuard
{
public:
    [[nodiscard]] explicit ScopeGuard(Function&& function) noexcept(std::is_nothrow_move_constructible_v<Function>)
        : m_function(std::move(function))
    { }

    [[nodiscard]] explicit ScopeGuard(const Function& function) noexcept(std::is_nothrow_copy_constructible_v<Function>)
        : m_function(function)
    { }

    [[nodiscard]] ScopeGuard(ScopeGuard&& other) noexcept(std::is_nothrow_move_constructible_v<Function>)
        : m_function(std::move(other.m_function))
        , m_invoke(std::exchange(other.m_invoke, false))
    { }

    ScopeGuard(const ScopeGuard&)            = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;
    ScopeGuard& operator=(ScopeGuard&&)      = delete;

    ~ScopeGuard() noexcept
    {
        if(m_invoke) {
            std::invoke(m_function);
        }
    }

    void dismiss() noexcept
    {
        m_invoke = false;
    }

    void commit() noexcept(std::is_nothrow_invocable_v<Function&>)
    {
        assert(m_invoke);
        m_invoke = false;
        std::invoke(m_function);
    }

private:
    Function m_function;
    bool m_invoke{true};
};

template <typename Result>
ScopeGuard(Result (&)()) -> ScopeGuard<Result (*)()>;

template <typename Function>
[[nodiscard]] auto scopeGuard(Function&& function)
{
    return ScopeGuard<std::decay_t<Function>>{std::forward<Function>(function)};
}
} // namespace Fooyin
