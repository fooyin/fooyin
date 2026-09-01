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

#include <memory>

namespace Fooyin::GlobalHotkeys {
class GlobalShortcutX11BackendPrivate;

class GlobalShortcutX11Backend : public GlobalShortcutBackend
{
public:
    explicit GlobalShortcutX11Backend(QObject* parent = nullptr);
    ~GlobalShortcutX11Backend() override;

    [[nodiscard]] GlobalShortcutAvailability availability() const override;
    void applyBindings(const GlobalShortcutDescriptorList& bindings) override;
    void clearBindings() override;

private:
    std::unique_ptr<GlobalShortcutX11BackendPrivate> p;
};
} // namespace Fooyin::GlobalHotkeys
