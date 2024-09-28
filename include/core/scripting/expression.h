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
    Date           = 5,
    Function       = 6,
    FunctionArg    = 7,
    Conditional    = 8,
    Not            = 9,
    Group          = 10,
    And            = 11,
    Or             = 12,
    XOr            = 13,
    Equals         = 14,
    Contains       = 15,
    Greater        = 16,
    GreaterEqual   = 17,
    Less           = 18,
    LessEqual      = 19,
    SortAscending  = 20,
    SortDescending = 21,
    All            = 22,
    QuotedLiteral  = 23,
    Missing        = 24,
    Present        = 25,
    Before         = 26,
    After          = 27,
    Since          = 28,
    During         = 29,
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
