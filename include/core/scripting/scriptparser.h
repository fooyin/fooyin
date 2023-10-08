/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include "expression.h"
#include "fycore_export.h"
#include "scriptregistry.h"

#include <QObject>

namespace Fy::Core::Scripting {
struct ParsedScript
{
    QString input;
    ExpressionList expressions;
    bool valid{false};
};

class FYCORE_EXPORT Parser
{
public:
    explicit Parser(Registry* registry);
    virtual ~Parser();

    ParsedScript parse(const QString& input);

    QString evaluate();
    QString evaluate(const ParsedScript& input, const Core::Track& track);
    QString evaluate(const ParsedScript& input);

    void setMetadata(const Core::Track& track);

protected:
    ScriptResult evalExpression(const Expression& exp) const;

    ScriptResult evalLiteral(const Expression& exp) const;
    ScriptResult evalVariable(const Expression& exp) const;
    ScriptResult evalVariableList(const Expression& exp) const;
    ScriptResult evalFunction(const Expression& exp) const;
    ScriptResult evalFunctionArg(const Expression& exp) const;
    ScriptResult evalConditional(const Expression& exp) const;

    Expression expression();
    Expression literal() const;
    Expression quote();
    Expression variable();
    Expression function();
    Expression functionArgs();
    Expression conditional();

    const ParsedScript& lastParsedScript() const;

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fy::Core::Scripting
