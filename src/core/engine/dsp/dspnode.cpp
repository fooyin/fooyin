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
void DspNode::reset() { }

void DspNode::flush(ProcessingBufferList& /*chunks*/, FlushMode /*mode*/) { }

AudioFormat DspNode::outputFormat(const AudioFormat& input) const
{
    return input;
}

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
} // namespace Fooyin
