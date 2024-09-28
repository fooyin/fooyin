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
    VariableRaw    = 4,
    Function       = 5,
    FunctionArg    = 6,
    Conditional    = 7,
    Not            = 8,
    Group          = 9,
    And            = 10,
    Or             = 11,
    Equals         = 12,
    Contains       = 13,
    Greater        = 14,
    GreaterEqual   = 15,
    Less           = 16,
    LessEqual      = 17,
    SortAscending  = 18,
    SortDescending = 19,
    All            = 20,
    QuotedLiteral  = 21,
    Missing        = 22,
    Present        = 23,
    Before         = 24,
    After          = 25,
    Since          = 26,
    During         = 27,
    Date           = 28,
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
