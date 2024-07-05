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

#include <QString>

namespace Fooyin {
namespace Expr {
enum Type : int
{
    Literal      = 0,
    Variable     = 1,
    VariableList = 2,
    Function     = 3,
    FunctionArg  = 4,
    Conditional  = 5,
    Null         = 6,
};
}

struct Expression;
using ExpressionList = std::vector<Expression>;

struct FuncValue
{
    QString name;
    ExpressionList args;
};

using ExpressionValue = std::variant<QString, FuncValue, ExpressionList>;

struct Expression
{
    Expr::Type type{Expr::Null};
    ExpressionValue value{QString{}};
};
} // namespace Fooyin
