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

#include "fyutils_export.h"

#include <QObject>
#include <QWidget>

namespace Fooyin {
class LogWidget;

class FYUTILS_EXPORT MessageHandler : public QObject
{
    Q_OBJECT

public:
    explicit MessageHandler(QObject* parent = nullptr);

    static void install(LogWidget* widget);
    static void handler(QtMsgType type, const QMessageLogContext&, const QString& msg);

    static QtMsgType level();
    static void setLevel(QtMsgType level);

signals:
    void showMessage(QString msg, QtMsgType type);

private:
    static MessageHandler* instance();
    std::atomic<QtMsgType> m_level;
};
} // namespace Fooyin

Q_DECLARE_METATYPE(QtMsgType);
