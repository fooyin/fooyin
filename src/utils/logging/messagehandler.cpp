/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include <QLoggingCategory>

#include <iostream>

namespace Fooyin {
namespace {
QString levelToFilterRule(MessageHandler::Level level)
{
    using namespace Qt::StringLiterals;
    using Level = MessageHandler::Level;

    switch(level) {
        case Level::Trace:
        case Level::Debug: {
            auto rules = u"fy.*.critical=true\nfy.*.warning=true\nfy.*.info=true\nfy.*.debug=true\n"_s;
            if(level == Level::Debug) {
                rules += u"fy.trace.*.debug=false\n"_s;
            }
            return rules;
        }
        case Level::Info:
            return u"fy.*.critical=true\nfy.*.warning=true\nfy.*.info=true\nfy.*.debug=false\n"_s;
        case Level::Warning:
            return u"fy.*.critical=true\nfy.*.warning=true\nfy.*.info=false\nfy.*.debug=false\n"_s;
        case Level::Critical:
            return u"fy.*.critical=true\nfy.*.warning=false\nfy.*.info=false\nfy.*.debug=false\n"_s;
        case Level::Fatal:
        default:
            return {};
    }
}

bool filterLogMessage(QtMsgType type, const QString& message)
{
    // Qt's Wayland backend emits this warning for non-popup mouse grabs
    if(type == QtWarningMsg
       && message == QLatin1StringView{"This plugin supports grabbing the mouse only for popup windows"}) {
        return true;
    }

    if((type == QtInfoMsg)
       && (message.startsWith(QLatin1StringView{"OpenType support missing for"})
           || message == QLatin1StringView{"libpng warning: eXIf: duplicate"}
           || message == QLatin1StringView{"libpng warning: sRGB: out of place"}
           || message.startsWith(QLatin1StringView{"libpng warning: iCCP: profile '"}))) {
        return true;
    }

    return false;
}
} // namespace

MessageHandler::MessageHandler(QObject* parent)
    : QObject{parent}
    , m_level{Level::Info}
{
    qRegisterMetaType<QtMsgType>();
}

void MessageHandler::install(LogWidget* widget)
{
    const auto* self = instance();
    qInstallMessageHandler(handler);

    if(widget) {
        connect(self, &MessageHandler::showMessage, widget, &LogWidget::addEntry, Qt::QueuedConnection);
    }
}

void MessageHandler::handler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    if(filterLogMessage(type, message)) {
        return;
    }

    const QString formattedMsg = qFormatLogMessage(type, context, message);
    const std::string msg      = formattedMsg.toLocal8Bit().constData();

    switch(type) {
        case QtDebugMsg:
        case QtInfoMsg:
        case QtWarningMsg:
            std::clog << msg << '\n';
            break;
        case QtCriticalMsg:
            std::cerr << msg << '\n';
            break;
        case QtFatalMsg:
            throw std::runtime_error(msg);
    }

    QMetaObject::invokeMethod(
        instance(), [formattedMsg, type]() { Q_EMIT instance()->showMessage(formattedMsg, type); },
        Qt::QueuedConnection);
}

MessageHandler::Level MessageHandler::level()
{
    return instance()->m_level.load(std::memory_order_acquire);
}

void MessageHandler::setLevel(Level level)
{
    QString filterRules;
    filterRules.append(levelToFilterRule(level));
    QLoggingCategory::setFilterRules(filterRules);
    instance()->m_level.store(level, std::memory_order_release);
}

MessageHandler* MessageHandler::instance()
{
    static MessageHandler instance;
    return &instance;
}
} // namespace Fooyin
