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

#include "dspsettingsregistry.h"

#include <algorithm>

namespace Fooyin {
void DspSettingsRegistry::registerProvider(std::unique_ptr<DspSettingsProvider> provider)
{
    if(!provider) {
        return;
    }

    const QString providerId = provider->id().trimmed();
    if(providerId.isEmpty()) {
        return;
    }

    auto it = std::ranges::find_if(
        m_providers, [&providerId](const auto& existing) { return existing && existing->id() == providerId; });
    if(it != m_providers.end()) {
        m_providerById.insert(providerId, provider.get());
        *it = std::move(provider);
        return;
    }

    m_providerById.insert(providerId, provider.get());
    m_providers.push_back(std::move(provider));
}

DspSettingsProvider* DspSettingsRegistry::providerFor(const QString& id) const
{
    const auto it = m_providerById.constFind(id);
    return it == m_providerById.constEnd() ? nullptr : it.value();
}

bool DspSettingsRegistry::hasProvider(const QString& id) const
{
    return m_providerById.contains(id);
}
} // namespace Fooyin
