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

#include "dspregistry.h"

#include "downmixtomonodsp.h"
#include "downmixtostereodsp.h"
#include "monotostereodsp.h"
#include "resamplerdsp.h"
#include "reversestereodsp.h"
#include "skipsilencedsp.h"

#include <algorithm>
#include <mutex>

using namespace Qt::StringLiterals;

namespace Fooyin {
DspRegistry::DspRegistry()
{
    registerBuiltins();
}

void DspRegistry::registerDsp(const DspNode::Entry& entry)
{
    const std::scoped_lock lock{m_mutex};

    const auto it = std::ranges::find_if(m_entries, [&entry](const auto& regEntry) { return regEntry.id == entry.id; });
    if(it != m_entries.end()) {
        return;
    }

    m_entries.push_back(entry);
}

std::vector<DspNode::Entry> DspRegistry::entries() const
{
    const std::scoped_lock lock{m_mutex};
    return m_entries;
}

std::unique_ptr<DspNode> DspRegistry::create(const QString& id) const
{
    const std::scoped_lock lock{m_mutex};

    const auto it = std::ranges::find_if(m_entries, [&id](const auto& entry) { return entry.id == id; });
    if(it != m_entries.end() && it->factory) {
        return it->factory();
    }

    return nullptr;
}

void DspRegistry::registerBuiltins()
{
    {
        auto dsp           = std::make_unique<SkipSilenceDsp>();
        const QString id   = dsp->id();
        const QString name = dsp->name();
        registerDsp({.id = id, .name = name, .factory = []() { return std::make_unique<SkipSilenceDsp>(); }});
    }
    {
        auto dsp           = std::make_unique<ResamplerDsp>();
        const QString id   = dsp->id();
        const QString name = u"Resampler (FFmpeg)"_s;
        registerDsp({.id = id, .name = name, .factory = []() { return std::make_unique<ResamplerDsp>(); }});
    }
    {
        auto dsp           = std::make_unique<MonoToStereoDsp>();
        const QString id   = dsp->id();
        const QString name = dsp->name();
        registerDsp({.id = id, .name = name, .factory = []() { return std::make_unique<MonoToStereoDsp>(); }});
    }
    {
        auto dsp           = std::make_unique<ReverseStereoDsp>();
        const QString id   = dsp->id();
        const QString name = dsp->name();
        registerDsp({.id = id, .name = name, .factory = []() { return std::make_unique<ReverseStereoDsp>(); }});
    }
    {
        auto dsp           = std::make_unique<DownmixToStereoDsp>();
        const QString id   = dsp->id();
        const QString name = dsp->name();
        registerDsp({.id = id, .name = name, .factory = []() { return std::make_unique<DownmixToStereoDsp>(); }});
    }
    {
        auto dsp           = std::make_unique<DownmixToMonoDsp>();
        const QString id   = dsp->id();
        const QString name = dsp->name();
        registerDsp({.id = id, .name = name, .factory = []() { return std::make_unique<DownmixToMonoDsp>(); }});
    }
}
} // namespace Fooyin
