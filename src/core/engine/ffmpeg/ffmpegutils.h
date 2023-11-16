/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

extern "C"
{
#include <libavutil/samplefmt.h>
}

class QString;
class AVFrame;

namespace Fooyin::Utils {
void printError(int error);
void printError(const QString& error);
AVSampleFormat interleaveFormat(AVSampleFormat planarFormat);
// Set data in an interleaved frame to the audio
// after the given number of samples
void skipSamples(AVFrame* frame, int samples);
void fillSilence(uint8_t* dst, int bytes, int format);
void adjustVolumeOfSamples(uint8_t* data, AVSampleFormat format, int bytes, double volume);
} // namespace Fooyin::Utils
