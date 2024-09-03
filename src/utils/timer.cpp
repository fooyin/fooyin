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

#include <utils/timer.h>

#include <iomanip>
#include <sstream>

namespace Fooyin {
void Timer::reset()
{
    m_start = Clock::now();
}

std::chrono::milliseconds Timer::elapsed() const
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - m_start);
}

QString Timer::elapsedFormatted() const
{
    auto elapsedMs     = elapsed();
    const auto minutes = std::chrono::duration_cast<std::chrono::minutes>(elapsedMs);
    elapsedMs -= minutes;
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsedMs);
    elapsedMs -= seconds;
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(elapsedMs);
    const auto microseconds = elapsedMs - milliseconds;

    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(1) << minutes.count() << ":" << std::setfill('0') << std::setw(2)
        << seconds.count() << "." << std::setfill('0') << std::setw(3) << milliseconds.count() << std::setfill('0')
        << std::setw(3) << microseconds.count();

    return QString::fromStdString(oss.str());
}
} // namespace Fooyin
