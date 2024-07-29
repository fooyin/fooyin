/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include <core/engine/audioformat.h>

extern "C"
{
#include <libavcodec/avcodec.h>
}

class QString;
class AVCodecParameters;

#define OLD_CHANNEL_LAYOUT (LIBAVCODEC_VERSION_INT < AV_VERSION_INT(59, 24, 100))
#define OLD_FRAME (LIBAVUTIL_VERSION_MAJOR < 58)

namespace Fooyin::Utils {
void printError(int error);
void printError(const QString& error);

SampleFormat sampleFormat(AVSampleFormat format, int bps);
AVSampleFormat sampleFormat(SampleFormat format);
AudioFormat audioFormatFromCodec(AVCodecParameters* codec);
} // namespace Fooyin::Utils
