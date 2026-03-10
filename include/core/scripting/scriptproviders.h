/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <LukeT1@proton.me>
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

#include <core/scripting/scripttypes.h>

#include <concepts>
#include <functional>
#include <initializer_list>
#include <type_traits>
#include <utility>
#include <vector>

namespace Fooyin {
using ScriptVariableInvokeFn = ScriptResult (*)(const ScriptContext&, const QString&);

struct ScriptVariableInvoker
{
    ScriptVariableInvokeFn invoke{nullptr};

    [[nodiscard]] ScriptResult operator()(const ScriptContext& context, const QString& name) const
    {
        return invoke ? invoke(context, name) : ScriptResult{};
    }

    [[nodiscard]] explicit operator bool() const
    {
        return invoke != nullptr;
    }
};

/*!
 * Describes one provider-defined variable.
 */
struct ScriptVariableDescriptor
{
    VariableKind kind;
    QString name;
    ScriptVariableInvoker invoker;
};

/*!
 * Evaluation payload passed to provider-defined functions.
 */
struct ScriptFunctionCallContext
{
    const ScriptContext* context{nullptr};
    std::span<const ScriptResult> args;
    ScriptSubject subject;
};

using ScriptFunctionInvokeFn = ScriptResult (*)(const ScriptFunctionCallContext&);

struct ScriptFunctionInvoker
{
    ScriptFunctionInvokeFn invoke{nullptr};

    [[nodiscard]] ScriptResult operator()(const ScriptFunctionCallContext& call) const
    {
        return invoke ? invoke(call) : ScriptResult{};
    }

    [[nodiscard]] explicit operator bool() const
    {
        return invoke != nullptr;
    }
};

/*!
 * Describes one provider-defined function.
 */
struct ScriptFunctionDescriptor
{
    QString name;
    ScriptFunctionInvoker invoker;
};

namespace ScriptFunction {
template <typename NewCntr, typename Cntr>
NewCntr containerCast(const Cntr& from)
{
    return NewCntr(std::begin(from), std::end(from));
}

template <auto Func>
concept SupportedFunctionCallable
    = std::invocable<decltype(Func), const QStringList&> || std::invocable<decltype(Func)>
   || std::invocable<decltype(Func), const Track&, const QStringList&> || std::invocable<decltype(Func), const Track&>
   || std::invocable<decltype(Func), const ScriptValueList&>
   || std::invocable<decltype(Func), const ScriptFunctionCallContext&>;

template <auto Func>
    requires SupportedFunctionCallable<Func>
ScriptResult invoke(const ScriptFunctionCallContext& call)
{
    if constexpr(std::is_invocable_r_v<QString, decltype(Func), const QStringList&>) {
        const QString value = std::invoke(Func, containerCast<QStringList>(call.args));
        return {.value = value, .cond = !value.isEmpty()};
    }
    else if constexpr(std::is_invocable_r_v<QString, decltype(Func)>) {
        const QString value = std::invoke(Func);
        return {.value = value, .cond = !value.isEmpty()};
    }
    else if constexpr(std::is_invocable_r_v<QString, decltype(Func), const Track&, const QStringList&>) {
        if(!call.subject.track) {
            return {};
        }
        const QString value = std::invoke(Func, *call.subject.track, containerCast<QStringList>(call.args));
        return {.value = value, .cond = !value.isEmpty()};
    }
    else if constexpr(std::is_invocable_r_v<QString, decltype(Func), const Track&>) {
        if(!call.subject.track) {
            return {};
        }
        const QString value = std::invoke(Func, *call.subject.track);
        return {.value = value, .cond = !value.isEmpty()};
    }
    else if constexpr(std::is_invocable_r_v<ScriptResult, decltype(Func), const QStringList&>) {
        return std::invoke(Func, containerCast<QStringList>(call.args));
    }
    else if constexpr(std::is_invocable_r_v<ScriptResult, decltype(Func), const ScriptValueList&>) {
        return std::invoke(Func, containerCast<ScriptValueList>(call.args));
    }
    else if constexpr(std::is_invocable_r_v<ScriptResult, decltype(Func), const ScriptFunctionCallContext&>) {
        return std::invoke(Func, call);
    }
    else if constexpr(std::is_invocable_r_v<QString, decltype(Func), const ScriptFunctionCallContext&>) {
        const QString value = std::invoke(Func, call);
        return {.value = value, .cond = !value.isEmpty()};
    }
    return {};
}
} // namespace ScriptFunction

namespace ScriptVariable {
template <auto Func>
concept SupportedVariableCallable
    = std::invocable<decltype(Func), const ScriptContext&, const QString&>
   || std::invocable<decltype(Func), const ScriptContext&> || std::invocable<decltype(Func)>;

template <auto Func>
    requires SupportedVariableCallable<Func>
ScriptResult invoke(const ScriptContext& context, const QString& name)
{
    if constexpr(std::is_invocable_r_v<ScriptResult, decltype(Func), const ScriptContext&, const QString&>) {
        return std::invoke(Func, context, name);
    }
    else if constexpr(std::is_invocable_r_v<QString, decltype(Func), const ScriptContext&, const QString&>) {
        const QString value = std::invoke(Func, context, name);
        return {.value = value, .cond = !value.isEmpty()};
    }
    else if constexpr(std::is_invocable_r_v<ScriptResult, decltype(Func), const ScriptContext&>) {
        return std::invoke(Func, context);
    }
    else if constexpr(std::is_invocable_r_v<QString, decltype(Func), const ScriptContext&>) {
        const QString value = std::invoke(Func, context);
        return {.value = value, .cond = !value.isEmpty()};
    }
    else if constexpr(std::is_invocable_r_v<ScriptResult, decltype(Func)>) {
        return std::invoke(Func);
    }
    else if constexpr(std::is_invocable_r_v<QString, decltype(Func)>) {
        const QString value = std::invoke(Func);
        return {.value = value, .cond = !value.isEmpty()};
    }
    return {};
}
} // namespace ScriptVariable

template <auto Func>
    requires ScriptVariable::SupportedVariableCallable<Func>
constexpr ScriptVariableInvoker makeScriptVariableInvoker()
{
    return {.invoke = &ScriptVariable::invoke<Func>};
}

template <auto Func>
[[nodiscard]] inline ScriptVariableDescriptor makeScriptVariableDescriptor(VariableKind kind, QString name)
{
    return {.kind = kind, .name = std::move(name), .invoker = makeScriptVariableInvoker<Func>()};
}

template <auto Func>
    requires ScriptFunction::SupportedFunctionCallable<Func>
constexpr ScriptFunctionInvoker makeScriptFunctionInvoker()
{
    return {.invoke = &ScriptFunction::invoke<Func>};
}

template <auto Func>
[[nodiscard]] inline ScriptFunctionDescriptor makeScriptFunctionDescriptor(QString name)
{
    return {.name = std::move(name), .invoker = makeScriptFunctionInvoker<Func>()};
}

class FYCORE_EXPORT ScriptVariableProvider
{
public:
    virtual ~ScriptVariableProvider() = default;

    [[nodiscard]] virtual std::span<const ScriptVariableDescriptor> variables() const = 0;
};

/*!
 * Convenience provider for a fixed set of variable descriptors.
 */
class StaticScriptVariableProvider : public ScriptVariableProvider
{
public:
    StaticScriptVariableProvider() = default;
    explicit StaticScriptVariableProvider(std::vector<ScriptVariableDescriptor> variables)
        : m_variables{std::move(variables)}
    { }

    StaticScriptVariableProvider(std::initializer_list<ScriptVariableDescriptor> variables)
        : m_variables{variables}
    { }

    [[nodiscard]] std::span<const ScriptVariableDescriptor> variables() const override
    {
        return m_variables;
    }

private:
    std::vector<ScriptVariableDescriptor> m_variables;
};

class FYCORE_EXPORT ScriptFunctionProvider
{
public:
    virtual ~ScriptFunctionProvider() = default;

    [[nodiscard]] virtual std::span<const ScriptFunctionDescriptor> functions() const = 0;
};

/*!
 * Convenience provider for a fixed set of function descriptors.
 */
class StaticScriptFunctionProvider : public ScriptFunctionProvider
{
public:
    StaticScriptFunctionProvider() = default;

    explicit StaticScriptFunctionProvider(std::vector<ScriptFunctionDescriptor> functions)
        : m_functions{std::move(functions)}
    { }

    StaticScriptFunctionProvider(std::initializer_list<ScriptFunctionDescriptor> functions)
        : m_functions{functions}
    { }

    [[nodiscard]] std::span<const ScriptFunctionDescriptor> functions() const override
    {
        return m_functions;
    }

private:
    std::vector<ScriptFunctionDescriptor> m_functions;
};
} // namespace Fooyin
