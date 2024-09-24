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
enum Type : uint8_t
{
    Null           = 0,
    Literal        = 1,
    Variable       = 2,
    VariableList   = 3,
    Function       = 4,
    FunctionArg    = 5,
    Conditional    = 6,
    Not            = 7,
    Group          = 8,
    And            = 9,
    Or             = 10,
    Equals         = 11,
    Contains       = 12,
    Greater        = 13,
    GreaterEqual   = 14,
    Less           = 15,
    LessEqual      = 16,
    SortAscending  = 17,
    SortDescending = 18,
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
