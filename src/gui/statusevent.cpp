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

#include "statusevent.h"

#include <utils/utils.h>

#include <QCoreApplication>
#include <QMainWindow>

namespace Fooyin {
const QEvent::Type StatusEvent::StatusEventType = static_cast<QEvent::Type>(QEvent::registerEventType());

StatusEvent::StatusEvent(QString message)
    : StatusEvent{std::move(message), -1}
{ }

StatusEvent::StatusEvent(QString message, int timeout)
    : QEvent{StatusEventType}
    , m_message{std::move(message)}
    , m_timeout{timeout}
{ }

void StatusEvent::post(QString message, int timeout)
{
    if(auto* window = Utils::getMainWindow()) {
        auto* event = new StatusEvent(std::move(message), timeout);
        QCoreApplication::postEvent(window, event);
    }
}

QString StatusEvent::message() const
{
    return m_message;
}

int StatusEvent::timeout() const
{
    return m_timeout;
}
} // namespace Fooyin
