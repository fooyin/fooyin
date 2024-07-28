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

#include "utils/logging/messagehandler.h"

#include "logging/logwidget.h"

#include <iostream>

namespace Fooyin {
MessageHandler::MessageHandler(QObject* parent)
    : QObject{parent}
{
    qRegisterMetaType<QtMsgType>();
}

void MessageHandler::install(LogWidget* widget)
{
    auto* self = MessageHandler::instance();
    qInstallMessageHandler(MessageHandler::handler);

    if(widget) {
        connect(self, &MessageHandler::showMessage, widget, &LogWidget::addEntry, Qt::QueuedConnection);
    }
}

void MessageHandler::handler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    const QString formattedMsg = qFormatLogMessage(type, context, message);
    const std::string msg      = formattedMsg.toLocal8Bit().constData();

    switch(type) {
        case(QtDebugMsg):
        case(QtInfoMsg):
            std::clog << msg << '\n';
            break;
        case(QtWarningMsg):
            std::clog << msg << '\n';
            break;
        case(QtCriticalMsg):
            std::cerr << msg << '\n';
            break;
        case(QtFatalMsg):
            throw std::runtime_error(msg);
    }

    QMetaObject::invokeMethod(
        instance(), [formattedMsg, type]() { emit instance() -> showMessage(formattedMsg, type); },
        Qt::QueuedConnection);
}

MessageHandler* MessageHandler::instance()
{
    static MessageHandler instance;
    return &instance;
}
} // namespace Fooyin
