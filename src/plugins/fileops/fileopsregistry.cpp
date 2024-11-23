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

#include "fileopsregistry.h"

#include <QRegularExpression>

using namespace Qt::StringLiterals;

namespace Fooyin {
ScriptResult FileOpsRegistry::value(const QString& var, const Track& track) const
{
    ScriptResult result = ScriptRegistry::value(var, track);
    result.value        = replaceSeparators(result.value);

    return result;
}

QString FileOpsRegistry::replaceSeparators(const QString& input)
{
    static const QRegularExpression regex{uR"([/\\])"_s};
    QString output{input};
    return output.replace(regex, "-"_L1);
}
} // namespace Fooyin
