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
#include <type_traits>
#include <vector>

namespace Fooyin {
using ScriptVariableInvokeFn = ScriptResult (*)(void*, const ScriptContext&, const QString&);

struct ScriptVariableInvoker
{
    ScriptVariableInvokeFn invoke{nullptr};
    void* userData{nullptr}; // Non-owning; valid until no running or future callback can use it

    [[nodiscard]] ScriptResult operator()(const ScriptContext& context, const QString& name) const
    {
        return invoke ? invoke(userData, context, name) : ScriptResult{};
    }

    [[nodiscard]] explicit operator bool() const
    {
        return invoke != nullptr;
    }
};

/*!
 * Describes one registered variable.
 */
struct ScriptVariableDescriptor
{
    VariableKind kind{VariableKind::Generic};
    QString name{};
    ScriptVariableInvoker invoker{};
    QString category{};
    QString description{};
    std::vector<QString> aliases{};
};

/*!
 * Evaluation payload passed to registered functions.
 */
struct ScriptFunctionCallContext
{
    const ScriptContext* context{nullptr};
    const ScriptEvaluationOptions* options{nullptr};
    std::span<const ScriptResult> args;
    ScriptSubject subject;
};

using ScriptFunctionInvokeFn = ScriptResult (*)(void*, const ScriptFunctionCallContext&);

struct ScriptFunctionInvoker
{
    ScriptFunctionInvokeFn invoke{nullptr};
    void* userData{nullptr}; // Non-owning; valid until no running or future callback can use it

    [[nodiscard]] ScriptResult operator()(const ScriptFunctionCallContext& call) const
    {
        return invoke ? invoke(userData, call) : ScriptResult{};
    }

    [[nodiscard]] explicit operator bool() const
    {
        return invoke != nullptr;
    }
};

/*!
 * Describes one registered function.
 */
struct ScriptFunctionDescriptor
{
    QString name{};
    ScriptFunctionInvoker invoker{};
    QString signature{};
    QString category{};
    QString description{};
    FunctionKind kind{FunctionKind::Generic};
    std::vector<QString> aliases{};
};

template <typename Callable, typename... Args>
concept ScriptCallable
    = std::is_invocable_r_v<ScriptResult, Callable&, Args...> || std::is_invocable_r_v<QString, Callable&, Args...>;

template <typename Callable, typename... Args>
    requires ScriptCallable<Callable, Args...>
ScriptResult invokeScriptCallable(Callable& callable, Args&&... args)
{
    if constexpr(std::is_invocable_r_v<ScriptResult, Callable&, Args...>) {
        return std::invoke(callable, std::forward<Args>(args)...);
    }
    else {
        const QString value = std::invoke(callable, std::forward<Args>(args)...);
        return {.value = value, .cond = !value.isEmpty()};
    }
}

namespace ScriptFunction {
template <typename NewCntr, typename Cntr>
NewCntr containerCast(const Cntr& from)
{
    return NewCntr(std::begin(from), std::end(from));
}

template <typename Callable>
concept SupportedFunctionCallable
    = ScriptCallable<Callable, const QStringList&> || ScriptCallable<Callable>
   || ScriptCallable<Callable, const Track&, const QStringList&> || ScriptCallable<Callable, const Track&>
   || ScriptCallable<Callable, const ScriptValueList&> || ScriptCallable<Callable, const ScriptFunctionCallContext&>;

template <typename Callable>
    requires SupportedFunctionCallable<Callable>
ScriptResult invoke(Callable&& callable, const ScriptFunctionCallContext& call)
{
    if constexpr(ScriptCallable<Callable, const QStringList&>) {
        const auto args = containerCast<QStringList>(call.args);
        return invokeScriptCallable(callable, args);
    }
    else if constexpr(ScriptCallable<Callable>) {
        return invokeScriptCallable(callable);
    }
    else if constexpr(ScriptCallable<Callable, const Track&, const QStringList&>) {
        if(!call.subject.track) {
            return {};
        }
        const auto args = containerCast<QStringList>(call.args);
        return invokeScriptCallable(callable, *call.subject.track, args);
    }
    else if constexpr(ScriptCallable<Callable, const Track&>) {
        if(!call.subject.track) {
            return {};
        }
        return invokeScriptCallable(callable, *call.subject.track);
    }
    else if constexpr(ScriptCallable<Callable, const ScriptValueList&>) {
        const auto args = containerCast<ScriptValueList>(call.args);
        return invokeScriptCallable(callable, args);
    }
    else {
        return invokeScriptCallable(callable, call);
    }
}
} // namespace ScriptFunction

namespace ScriptVariable {
template <typename Callable>
concept SupportedVariableCallable = ScriptCallable<Callable, const ScriptContext&, const QString&>
                                 || ScriptCallable<Callable, const ScriptContext&> || ScriptCallable<Callable>;

template <typename Callable>
    requires SupportedVariableCallable<Callable>
ScriptResult invoke(Callable&& callable, const ScriptContext& context, const QString& name)
{
    if constexpr(ScriptCallable<Callable, const ScriptContext&, const QString&>) {
        return invokeScriptCallable(callable, context, name);
    }
    else if constexpr(ScriptCallable<Callable, const ScriptContext&>) {
        return invokeScriptCallable(callable, context);
    }
    else {
        return invokeScriptCallable(callable);
    }
}
} // namespace ScriptVariable

template <auto Func>
    requires ScriptVariable::SupportedVariableCallable<decltype(Func)>
constexpr ScriptVariableInvoker makeScriptVariableInvoker()
{
    return {.invoke = [](void*, const ScriptContext& context, const QString& name) {
        return ScriptVariable::invoke(Func, context, name);
    }};
}

template <auto Func, typename State>
    requires ScriptVariable::SupportedVariableCallable<decltype(std::bind_front(Func, std::declval<State*>()))>
constexpr ScriptVariableInvoker makeScriptVariableInvoker(State* userData)
{
    if(!userData) {
        return {};
    }
    return {.invoke =
                [](void* data, const ScriptContext& context, const QString& name) {
                    return ScriptVariable::invoke(std::bind_front(Func, static_cast<State*>(data)), context, name);
                },
            .userData = const_cast<std::remove_const_t<State>*>(userData)};
}

template <auto Func>
    requires ScriptVariable::SupportedVariableCallable<decltype(Func)>
[[nodiscard]] ScriptVariableDescriptor makeScriptVariableDescriptor(VariableKind kind, QString name,
                                                                    QString category = {}, QString description = {},
                                                                    std::vector<QString> aliases = {})
{
    return {.kind        = kind,
            .name        = std::move(name),
            .invoker     = makeScriptVariableInvoker<Func>(),
            .category    = std::move(category),
            .description = std::move(description),
            .aliases     = std::move(aliases)};
}

template <auto Func, typename State>
    requires ScriptVariable::SupportedVariableCallable<decltype(std::bind_front(Func, std::declval<State*>()))>
[[nodiscard]] ScriptVariableDescriptor makeScriptVariableDescriptor(State* userData, VariableKind kind, QString name,
                                                                    QString category = {}, QString description = {},
                                                                    std::vector<QString> aliases = {})
{
    return {.kind        = kind,
            .name        = std::move(name),
            .invoker     = makeScriptVariableInvoker<Func>(userData),
            .category    = std::move(category),
            .description = std::move(description),
            .aliases     = std::move(aliases)};
}

template <auto Func>
    requires ScriptFunction::SupportedFunctionCallable<decltype(Func)>
constexpr ScriptFunctionInvoker makeScriptFunctionInvoker()
{
    return {.invoke = [](void*, const ScriptFunctionCallContext& call) { return ScriptFunction::invoke(Func, call); }};
}

template <auto Func, typename State>
    requires ScriptFunction::SupportedFunctionCallable<decltype(std::bind_front(Func, std::declval<State*>()))>
constexpr ScriptFunctionInvoker makeScriptFunctionInvoker(State* userData)
{
    if(!userData) {
        return {};
    }
    return {.invoke =
                [](void* data, const ScriptFunctionCallContext& call) {
                    return ScriptFunction::invoke(std::bind_front(Func, static_cast<State*>(data)), call);
                },
            .userData = const_cast<std::remove_const_t<State>*>(userData)};
}

template <auto Func>
    requires ScriptFunction::SupportedFunctionCallable<decltype(Func)>
[[nodiscard]] ScriptFunctionDescriptor makeScriptFunctionDescriptor(QString name, QString signature = {},
                                                                    QString category = {}, QString description = {},
                                                                    std::vector<QString> aliases = {})
{
    return {.name        = std::move(name),
            .invoker     = makeScriptFunctionInvoker<Func>(),
            .signature   = std::move(signature),
            .category    = std::move(category),
            .description = std::move(description),
            .aliases     = std::move(aliases)};
}

template <auto Func, typename State>
    requires ScriptFunction::SupportedFunctionCallable<decltype(std::bind_front(Func, std::declval<State*>()))>
[[nodiscard]] ScriptFunctionDescriptor
makeScriptFunctionDescriptor(State* userData, QString name, QString signature = {}, QString category = {},
                             QString description = {}, std::vector<QString> aliases = {})
{
    return {.name        = std::move(name),
            .invoker     = makeScriptFunctionInvoker<Func>(userData),
            .signature   = std::move(signature),
            .category    = std::move(category),
            .description = std::move(description),
            .aliases     = std::move(aliases)};
}
} // namespace Fooyin
