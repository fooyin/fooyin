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
#include "scriptregistry.h"
#include "scriptscanner.h"

#include <QObject>

namespace Fy::Core::Scripting {
struct ParsedScript
{
    QString input;
    ExpressionList expressions;
    bool valid{false};
};

class Parser
{
public:
    virtual ~Parser() = default;

    ParsedScript parse(const QString& input);

    QString evaluate();
    QString evaluate(const ParsedScript& input, Track* track);

    virtual QString evaluate(const ParsedScript& input);

    void setMetadata(Track* track);

protected:
    ScriptResult evalExpression(const Expression& exp) const;

    virtual ScriptResult evalLiteral(const Expression& exp) const;
    virtual ScriptResult evalVariable(const Expression& exp) const;
    virtual ScriptResult evalFunction(const Expression& exp) const;
    virtual ScriptResult evalFunctionArg(const Expression& exp) const;
    virtual ScriptResult evalConditional(const Expression& exp) const;

    virtual Expression expression();
    virtual Expression literal() const;
    virtual Expression quote();
    virtual Expression variable();
    virtual Expression function();
    virtual Expression functionArgs();
    virtual Expression conditional();

    const Registry& registry() const;
    const ParsedScript& lastParsedScript() const;

    void advance();
    void consume(TokenType type, const QString& message);
    bool currentToken(TokenType type) const;
    bool match(TokenType type);
    void errorAtCurrent(const QString& message);
    void errorAt(const Token& token, const QString& message);
    void error(const QString& message);

private:
    Scanner m_scanner;
    Registry m_registry;

    Token m_current;
    Token m_previous;
    ParsedScript m_parsedScript;
    QString m_result;
    bool m_hadError;
};
} // namespace Fy::Core::Scripting
