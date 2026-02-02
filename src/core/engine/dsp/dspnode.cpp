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

#include <core/engine/dsp/dspnode.h>

namespace Fooyin {
DspNode::DspNode()
    : m_instanceId{0}
    , m_enabled{true}
{ }

bool DspNode::supportsLiveSettings() const
{
    return false;
}

AudioFormat DspNode::outputFormat(const AudioFormat& input) const
{
    return input;
}

void DspNode::reset() { }

void DspNode::flush(ProcessingBufferList& /*chunks*/, FlushMode /*mode*/) { }

QByteArray DspNode::saveSettings() const
{
    return {};
}

bool DspNode::loadSettings(const QByteArray& /*preset*/)
{
    return false;
}

int DspNode::latencyFrames() const
{
    return 0;
}

uint64_t DspNode::instanceId() const
{
    return m_instanceId;
}

void DspNode::setInstanceId(uint64_t instanceId)
{
    m_instanceId = instanceId;
}

bool DspNode::isEnabled() const
{
    return m_enabled;
}

void DspNode::setEnabled(bool enabled)
{
    m_enabled = enabled;
}
} // namespace Fooyin
