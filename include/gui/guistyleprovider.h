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

#include "fygui_export.h"
#include "guisettings.h"

#include <QObject>

namespace Fooyin {
class SettingsManager;

template <typename Callback>
concept StyleCallback = std::invocable<Callback> || std::invocable<Callback, const ResolvedAppStyle&>;
template <typename Receiver, typename Method>
concept StyleMemberCallback
    = std::derived_from<Receiver, QObject>
   && (std::invocable<Method, Receiver*> || std::invocable<Method, Receiver*, const ResolvedAppStyle&>);

class FYGUI_EXPORT GuiStyleProvider : public QObject
{
    Q_OBJECT

public:
    explicit GuiStyleProvider(SettingsManager* settings, QObject* parent = nullptr);

    [[nodiscard]] bool isResolved() const;
    [[nodiscard]] const ResolvedAppStyle& style() const;
    [[nodiscard]] QPalette palette() const;
    [[nodiscard]] QFont font(const QString& className = {}) const;

    template <StyleCallback Callback>
    void subscribe(QObject* context, Callback&& callback, bool replayCurrent = true) const
    {
        using CallbackType = std::remove_cvref_t<Callback>;

        auto wrapper = [cb = std::forward<Callback>(callback)](const ResolvedAppStyle& style) mutable {
            if constexpr(std::invocable<CallbackType&, const ResolvedAppStyle&>) {
                std::invoke(cb, style);
            }
            else {
                std::invoke(cb);
            }
        };

        QObject::connect(this, &GuiStyleProvider::styleChanged, context, std::move(wrapper));

        if(replayCurrent && m_isResolved) {
            if constexpr(std::invocable<Callback&, const ResolvedAppStyle&>) {
                std::invoke(callback, m_style);
            }
            else {
                std::invoke(callback);
            }
        }
    }

    template <typename Receiver, typename Method>
        requires StyleMemberCallback<Receiver, Method>
    void subscribe(Receiver* receiver, Method method, bool replayCurrent = true) const
    {
        QObject::connect(this, &GuiStyleProvider::styleChanged, receiver, method);

        if(replayCurrent && m_isResolved) {
            if constexpr(std::invocable<Method, Receiver*, const ResolvedAppStyle&>) {
                std::invoke(method, receiver, m_style);
            }
            else {
                std::invoke(method, receiver);
            }
        }
    }

Q_SIGNALS:
    void styleChanged(const Fooyin::ResolvedAppStyle& style);

private:
    void updateStyle(const ResolvedAppStyle& style);

    ResolvedAppStyle m_style;
    bool m_isResolved;
};
} // namespace Fooyin
