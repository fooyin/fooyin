/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "fycore_export.h"

#include "expression.h"
#include "scriptregistry.h"

#include <QObject>

namespace Fooyin {
class ScriptParserPrivate;

struct ScriptError
{
    int position;
    QString value;
    QString message;
};
using ErrorList = std::vector<ScriptError>;

struct ParsedScript
{
    QString input;
    ExpressionList expressions;
    ErrorList errors;

    [[nodiscard]] bool isValid() const
    {
        return errors.empty();
    }
};

class FYCORE_EXPORT ScriptParser
{
public:
    ScriptParser();
    explicit ScriptParser(ScriptRegistry* registry);
    virtual ~ScriptParser();

    ParsedScript parse(const QString& input);
    ParsedScript parse(const QString& input, const Track& track);
    ParsedScript parse(const QString& input, const TrackList& tracks);

    QString evaluate(const QString& input);
    QString evaluate(const ParsedScript& input);

    QString evaluate(const QString& input, const Track& track);
    QString evaluate(const ParsedScript& input, const Track& track);

    QString evaluate(const QString& input, const TrackList& tracks);
    QString evaluate(const ParsedScript& input, const TrackList& tracks);

    void clearCache();

private:
    std::unique_ptr<ScriptParserPrivate> p;
};
} // namespace Fooyin
