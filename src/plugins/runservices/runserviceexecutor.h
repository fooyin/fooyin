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

#include "runservices.h"

#include <core/track.h>

#include <QStringList>

#include <optional>

namespace Fooyin::RunServices {
enum class RunServiceCommandType : uint8_t
{
    Application = 0,
    Url,
};

struct RunServiceCommand
{
    RunServiceCommandType type{RunServiceCommandType::Application};
    QString target;
    QStringList arguments;

    bool operator==(const RunServiceCommand&) const = default;
};

QString evaluateRunServiceLabel(const RunService& service, const Track& track);
QStringList evaluateRunServiceCommands(const RunService& service, const TrackList& tracks);
std::optional<RunServiceCommand> resolveRunServiceCommand(const QString& command);
bool launchRunServiceCommand(const RunServiceCommand& command);
} // namespace Fooyin::RunServices
