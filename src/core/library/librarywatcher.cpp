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

#include "librarywatcher.h"

#include <QTimer>
#include <QTimerEvent>

using namespace std::chrono_literals;

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
constexpr auto Interval = 1000ms;
#else
constexpr auto Interval = 1000;
#endif

namespace Fooyin {
LibraryWatcher::LibraryWatcher(QObject* parent)
    : QFileSystemWatcher{parent}
{
    QObject::connect(this, &QFileSystemWatcher::directoryChanged, this, [this](const QString& path) {
        m_dirs.emplace(path);
        m_timer.start(Interval, this);
    });
}

void LibraryWatcher::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_timer.timerId()) {
        m_timer.stop();

        const QStringList paths{m_dirs.cbegin(), m_dirs.cend()};
        m_dirs.clear();
        emit libraryDirsChanged(paths);
    }
    QFileSystemWatcher::timerEvent(event);
}
} // namespace Fooyin

#include "moc_librarywatcher.cpp"
