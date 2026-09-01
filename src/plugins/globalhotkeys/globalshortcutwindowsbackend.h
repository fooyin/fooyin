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

#include <QAbstractNativeEventFilter>

#include <unordered_map>

namespace Fooyin::GlobalHotkeys {
class GlobalShortcutWindowsBackend : public GlobalShortcutBackend,
                                     private QAbstractNativeEventFilter
{
public:
    explicit GlobalShortcutWindowsBackend(QObject* parent = nullptr);
    ~GlobalShortcutWindowsBackend() override;

    [[nodiscard]] GlobalShortcutAvailability availability() const override;
    void applyBindings(const GlobalShortcutDescriptorList& bindings) override;
    void clearBindings() override;

private:
    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

    std::unordered_map<int, GlobalShortcutDescriptor> m_bindings;
    int m_nextNativeId;
};
} // namespace Fooyin::GlobalHotkeys
