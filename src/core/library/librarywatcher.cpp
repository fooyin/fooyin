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

using namespace std::chrono_literals;

constexpr auto Interval = 15ms;

namespace Fooyin {
LibraryWatcher::LibraryWatcher(QObject* parent)
    : QFileSystemWatcher{parent}
    , m_timer{new QTimer(this)}
{
    m_timer->setSingleShot(true);
    m_timer->setInterval(Interval);

    QObject::connect(this, &QFileSystemWatcher::directoryChanged, this, [this](const QString& path) {
        if(!m_dir.isEmpty() && path != m_dir) {
            m_timer->stop();
            emit libraryDirChanged(m_dir);
        }
        m_dir = path;
        m_timer->start();
    });

    QObject::connect(m_timer, &QTimer::timeout, this, [this]() {
        emit libraryDirChanged(m_dir);
        m_dir = {};
    });
}
} // namespace Fooyin

#include "moc_librarywatcher.cpp"
