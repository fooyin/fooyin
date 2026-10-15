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

#pragma once

#include "fycore_export.h"
#include <core/scripting/scriptdefs.h>

#include <memory>

namespace Fooyin {
class ScriptRegistryPrivate;
class ScriptRuntime;
/*!
 * Shared scripting catalogue. Register on the startup thread, then finalise before evaluation.
 * Resolvers may run concurrently on scripting workers; use snapshots or thread-safe plugin state.
 * Callback code and any plugin state it references must remain valid until shutdown() completes.
 * Invokers called directly outside the parser must obey the same lifetime contract.
 */
class FYCORE_EXPORT ScriptRegistry
{
public:
    ScriptRegistry();
    ~ScriptRegistry();
    ScriptRegistry(const ScriptRegistry&)            = delete;
    ScriptRegistry& operator=(const ScriptRegistry&) = delete;

    static ScriptRegistry& instance();

    bool registerVariable(ScriptVariableDescriptor variable);
    bool registerFunction(ScriptFunctionDescriptor function);
    void finalise();
    [[nodiscard]] bool isFinalised() const;
    //! Stop parsing/evaluation and clear the catalogue. Cease other catalogue access first; never call from a callback.
    void shutdown();

    [[nodiscard]] std::span<const ScriptVariableDescriptor> variables() const;
    [[nodiscard]] std::span<const ScriptFunctionDescriptor> functions() const;
    [[nodiscard]] VariableKind resolveVariable(const QString& name) const;
    [[nodiscard]] bool isVariable(const QString& name) const;
    [[nodiscard]] ScriptFunctionId resolveFunctionId(const QString& name) const;
    [[nodiscard]] FunctionKind functionKind(ScriptFunctionId id) const;
    [[nodiscard]] const ScriptVariableInvoker* variableInvoker(const QString& name) const;
    [[nodiscard]] const ScriptFunctionInvoker* functionInvoker(ScriptFunctionId id) const;

private:
    friend class ScriptRuntime;

    bool beginOperation();
    void endOperation();

    std::unique_ptr<ScriptRegistryPrivate> p;
};
} // namespace Fooyin
