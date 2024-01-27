/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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
#include <libavutil/samplefmt.h>
}

class QString;
class AVCodecParameters;

namespace Fooyin {
class AudioBuffer;

namespace Utils {
void printError(int error);
void printError(const QString& error);
AVSampleFormat interleaveFormat(AVSampleFormat planarFormat);
AudioFormat audioFormatFromCodec(AVCodecParameters* codec);
void adjustVolumeOfSamples(AudioBuffer& buffer, double volume);
} // namespace Utils
} // namespace Fooyin
