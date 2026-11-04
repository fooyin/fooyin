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

#include "filterscripting.h"

#include <core/scripting/scriptregistry.h>

#include <QCoreApplication>

using namespace Qt::StringLiterals;

namespace Fooyin::Filters {
namespace {
ScriptResult filterColumn(const ScriptFunctionCallContext& call)
{
    if(call.args.size() != 1 || !call.context) {
        return {};
    }

    bool valid{false};
    const int index = call.args.front().value.toInt(&valid);
    if(!valid || index <= 0) {
        return {};
    }

    const QString name = u"_FOOYIN_FILTER_COLUMN_%1"_s.arg(index);
    for(const auto& value : call.context->values) {
        if(value.name == name) {
            auto result = value.result;
            if(call.options && call.options->escapeRichText) {
                result.value.replace(u'\\', u"\\\\"_s);
                result.value.replace(u'<', u"\\<"_s);
            }
            return result;
        }
    }
    return {};
}
} // namespace

void registerScriptFunctions()
{
    ScriptRegistry::instance().registerFunction(
        {.name        = u"filter_column"_s,
         .invoker     = makeScriptFunctionInvoker<filterColumn>(),
         .signature   = u"$filter_column(index)"_s,
         .category    = QCoreApplication::translate("Fooyin", "Filter"),
         .description = QCoreApplication::translate(
             "Fooyin", "Filter column value at a one-based index; empty outside the current range"),
         .dependencies = {.unclassified = false}});
}
} // namespace Fooyin::Filters
