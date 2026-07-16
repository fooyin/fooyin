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

#include <gui/dsp/dspsettingsprovider.h>

#include <memory>
#include <unordered_map>
#include <vector>

namespace Fooyin {
class FYGUI_EXPORT DspSettingsRegistry
{
public:
    void registerProvider(std::unique_ptr<DspSettingsProvider> provider);

    [[nodiscard]] DspSettingsProvider* providerFor(const QString& id) const;
    [[nodiscard]] std::vector<DspSettingsProvider*> providers() const;
    [[nodiscard]] bool hasProvider(const QString& id) const;

    [[nodiscard]] static QString layoutWidgetKey(const QString& id);

private:
    std::unordered_map<QString, DspSettingsProvider*> m_providerById;
    std::vector<std::unique_ptr<DspSettingsProvider>> m_providers;
};
} // namespace Fooyin
