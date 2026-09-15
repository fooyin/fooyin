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

#include "runserviceexecutor.h"

#include <core/scripting/scriptparser.h>

#include <QDesktopServices>
#include <QFileInfo>
#include <QProcess>
#include <QUrl>

namespace Fooyin::RunServices {
namespace {
bool hasUrlScheme(const QString& value)
{
    const QUrl url{value};
    if(!url.isValid() || url.scheme().isEmpty()) {
        return false;
    }

#ifdef Q_OS_WIN
    if(url.scheme().size() == 1 && value.size() > 2 && value.at(1) == u':') {
        return false;
    }
#endif

    return true;
}
} // namespace

QString evaluateRunServiceLabel(const RunService& service, const Track& track)
{
    ScriptParser parser;
    const QString label = parser.evaluate(service.name, track).trimmed();
    return label.isEmpty() ? service.name : label;
}

QStringList evaluateRunServiceCommands(const RunService& service, const TrackList& tracks)
{
    const size_t runCount = std::min(tracks.size(), static_cast<size_t>(std::max(1, service.simultaneousRuns)));

    QStringList commands;
    commands.reserve(static_cast<qsizetype>(runCount));

    ScriptParser parser;
    const ParsedScript pathScript = parser.parse(service.path);
    if(!pathScript.isValid()) {
        return commands;
    }

    for(size_t i{0}; i < runCount; ++i) {
        QString command = parser.evaluate(pathScript, tracks.at(i)).trimmed();
        if(!command.isEmpty()) {
            commands.emplace_back(std::move(command));
        }
    }

    return commands;
}

std::optional<RunServiceCommand> resolveRunServiceCommand(const QString& command)
{
    const QString trimmed = command.trimmed();
    if(trimmed.isEmpty()) {
        return {};
    }

    if(QFileInfo::exists(trimmed)) {
        return RunServiceCommand{.type      = RunServiceCommandType::Url,
                                 .target    = QUrl::fromLocalFile(QFileInfo{trimmed}.absoluteFilePath()).toString(),
                                 .arguments = {}};
    }

    QStringList parts = QProcess::splitCommand(trimmed);
    if(parts.empty()) {
        return {};
    }

    if(parts.size() == 1) {
        const QString& target = parts.front();
        if(QFileInfo::exists(target)) {
            return RunServiceCommand{.type      = RunServiceCommandType::Url,
                                     .target    = QUrl::fromLocalFile(QFileInfo{target}.absoluteFilePath()).toString(),
                                     .arguments = {}};
        }
        if(hasUrlScheme(target)) {
            return RunServiceCommand{.type = RunServiceCommandType::Url, .target = target, .arguments = {}};
        }
    }

    RunServiceCommand resolved;
    resolved.target    = parts.takeFirst();
    resolved.arguments = std::move(parts);
    return resolved;
}

bool launchRunServiceCommand(const RunServiceCommand& command)
{
    if(command.type == RunServiceCommandType::Url) {
        return QDesktopServices::openUrl(QUrl::fromUserInput(command.target));
    }

    QProcess process;
    process.setProgram(command.target);
    process.setArguments(command.arguments);
    return process.startDetached();
}
} // namespace Fooyin::RunServices
