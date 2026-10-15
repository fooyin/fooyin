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

#include <core/engine/audioloader.h>
#include <core/engine/visualisationservice.h>
#include <core/track.h>
#include <utils/worker.h>

#include <array>
#include <atomic>
#include <vector>

namespace Fooyin::Spectrogram {
using SpectrogramColumn = std::array<VisualisationSession::SpectrumWindow, 2>;

struct SpectrogramBatch
{
    int firstColumn{0};
    int totalColumns{0};
    int channels{0};
    std::vector<SpectrogramColumn> columns;
};

class SpectrogramGenerator : public Worker
{
    Q_OBJECT

public:
    explicit SpectrogramGenerator(std::shared_ptr<AudioLoader> audioLoader);

    uint64_t cancel();

    void generate(uint64_t request, const Track& track, int fftSize, int timeColumns,
                  VisualisationSession::SpectrumWindowFunction windowFunction, bool mono, int firstChannel,
                  int channelCount, std::stop_token token);

Q_SIGNALS:
    void columnsReady(uint64_t request, const Fooyin::Spectrogram::SpectrogramBatch& batch);
    void finished(uint64_t request, const QString& error);

private:
    std::shared_ptr<AudioLoader> m_audioLoader;
    std::atomic<uint64_t> m_request;
};
} // namespace Fooyin::Spectrogram

Q_DECLARE_METATYPE(Fooyin::Spectrogram::SpectrogramBatch)
