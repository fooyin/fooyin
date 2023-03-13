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

#include "scriptparser.h"

#include <QDebug>

namespace Fy::Core::Scripting {
void Parser::parse(const QString& input)
{
    m_scanner.setup(input);
    m_nodes.clear();

    Expression exp = m_scanner.scanNext();
    while(exp.type != Null) {
        m_nodes.emplace_back(exp);
        exp = m_scanner.scanNext();
    }
}

QString Parser::evaluate()
{
    QString ret;
    for(auto& node : m_nodes) {
        ret.append(evaluate(&node).value);
    }
    return ret;
}

Value Parser::evaluate(Expression* exp)
{
    switch(exp->type) {
        case Literal: {
            return {std::get<Literal>(exp->value).value, true};
        }
        case Variable: {
            return {m_registry.varValue(std::get<Variable>(exp->value).value)};
        }
        case Function: {
            auto value = std::get<Function>(exp->value);
            ValueList args;
            for(auto& arg : value.args) {
                args.emplace_back(evaluate(&arg));
            }
            return {m_registry.function(value.name, args)};
        }
        case Null:
            break;
    }
    return {};
}
} // namespace Fy::Core::Scripting
