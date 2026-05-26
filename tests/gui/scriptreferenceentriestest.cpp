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

#include <core/scripting/scriptproviders.h>
#include <gui/scripting/scriptvariableregistry.h>

#include <gtest/gtest.h>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace Fooyin::Testing {
namespace {
const StaticScriptVariableProvider PluginReferenceVariableProvider{makeScriptVariableDescriptor<[]() -> ScriptResult {
    return {.value = u"x"_s, .cond = true};
}>(VariableKind::Generic, u"PLUGIN_REFERENCE_TEST_VARIABLE"_s, u"Plugin Test"_s, u"Shown in completion"_s)};
} // namespace

TEST(ScriptReferenceEntriesTest, IncludesGloballyRegisteredPluginVariables)
{
    ScriptVariableRegistry registry;
    registry.registerProvider(PluginReferenceVariableProvider);

    const auto variables = registry.variables();
    const auto it        = std::ranges::find_if(variables, [](const ScriptVariableDescriptor& variable) {
        return variable.name == u"PLUGIN_REFERENCE_TEST_VARIABLE"_s;
    });

    ASSERT_NE(it, variables.cend());
    EXPECT_EQ(VariableKind::Generic, it->kind);
    EXPECT_EQ(u"Plugin Test", it->category);
    EXPECT_EQ(u"Shown in completion", it->description);
}
} // namespace Fooyin::Testing
