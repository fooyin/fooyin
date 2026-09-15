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

#include "globalshortcutbackend.h"
#include "globalshortcutrepeater.h"

#include <Carbon/Carbon.h>

#include <unordered_map>

namespace Fooyin::GlobalHotkeys {
class GlobalShortcutMacosBackend : public GlobalShortcutBackend
{
public:
    explicit GlobalShortcutMacosBackend(QObject* parent = nullptr);
    ~GlobalShortcutMacosBackend() override;

    [[nodiscard]] GlobalShortcutAvailability availability() const override;
    void applyBindings(const GlobalShortcutDescriptorList& bindings) override;
    void clearBindings() override;

private:
    static OSStatus handleEvent(EventHandlerCallRef nextHandler, EventRef event, void* context);

    EventHandlerRef m_eventHandler;
    std::unordered_map<std::uint32_t, EventHotKeyRef> m_hotKeys;
    std::unordered_map<std::uint32_t, GlobalShortcutDescriptor> m_bindings;
    GlobalShortcutRepeater m_repeater;
    std::uint32_t m_nextNativeId;
};
} // namespace Fooyin::GlobalHotkeys
