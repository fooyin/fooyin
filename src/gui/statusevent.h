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

#pragma once

#include "fygui_export.h"

#include <QEvent>
#include <QString>

namespace Fooyin {
class FYGUI_EXPORT StatusEvent : public QEvent
{
public:
    static const QEvent::Type StatusEventType;

    explicit StatusEvent(QString message);
    StatusEvent(QString message, int timeout);

    static void post(QString message, int timeout = -1);

    [[nodiscard]] QString message() const;
    [[nodiscard]] int timeout() const;

private:
    QString m_message;
    int m_timeout;
};
} // namespace Fooyin
