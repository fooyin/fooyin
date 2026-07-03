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

#include "fycore_export.h"

#include <core/engine/audioformat.h>

#include <QObject>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace Fooyin {
class EngineHandler;
class VisualisationBackend;
class VisualisationSessionPrivate;
class VisualisationService;
class VisualisationServicePrivate;

/*!
 * Pull-based visualisation session backed by shared post-master-DSP,
 * pre-output-fader PCM history.
 *
 * Sessions are intended for in-app visual widgets that query PCM or spectrum
 * data on demand. This keeps widget repaint timing separate from engine audio
 * analysis and allows multiple widgets to share backend state.
 *
 * For push-style consumers that want fixed-hop analysis snapshots as they are
 * produced, use `EngineController::levelReady` / `EngineController::pcmReady`
 * instead.
 */
class FYCORE_EXPORT VisualisationSession
{
public:
    enum class SpectrumWindowFunction : uint8_t
    {
        Hann = 0,
        BlackmanHarris,
        None
    };

    enum class ChannelMode : uint8_t
    {
        Default = 0,
        Mono
    };

    struct ChannelSelection
    {
        enum class MixMode : uint8_t
        {
            AllChannels = 0,
            MonoAverage,
            SingleChannel,
            Mid,
            Side,
        };

        MixMode mixMode{MixMode::AllChannels};
        int channelIndex{0};

        [[nodiscard]] auto operator<=>(const ChannelSelection&) const = default;
    };

    struct PcmWindow
    {
        std::vector<float> samples;
        AudioFormat format{SampleFormat::F32, 0, 0};
        int frameCount{0};
        uint64_t startTimeMs{0};

        [[nodiscard]] size_t sampleCount() const
        {
            if(frameCount <= 0 || format.channelCount() <= 0) {
                return 0;
            }

            return static_cast<size_t>(frameCount) * static_cast<size_t>(format.channelCount());
        }

        [[nodiscard]] std::span<const float> interleavedSamples() const
        {
            return {samples.data(), sampleCount()};
        }

        [[nodiscard]] bool isValid() const
        {
            return frameCount > 0 && format.isValid() && sampleCount() <= samples.size();
        }
    };

    struct SpectrumWindow
    {
        std::vector<float> magnitudes;
        int fftSize{0};
        int sampleRate{0};
        uint64_t startTimeMs{0};

        [[nodiscard]] int binCount() const
        {
            return fftSize > 0 ? ((fftSize / 2) + 1) : 0;
        }

        [[nodiscard]] std::span<const float> bins() const
        {
            return {magnitudes.data(), static_cast<size_t>(binCount())};
        }

        [[nodiscard]] bool isValid() const
        {
            return fftSize > 1 && sampleRate > 0 && static_cast<size_t>(binCount()) <= magnitudes.size();
        }
    };

    ~VisualisationSession();

    VisualisationSession(const VisualisationSession&)            = delete;
    VisualisationSession& operator=(const VisualisationSession&) = delete;

    void requestBacklog(uint64_t durationMs);
    void setChannelSelection(ChannelSelection selection);
    //! Resample returned PCM windows to this rate; set to 0 to preserve the source rate.
    void setPcmTargetSampleRate(int sampleRate);

    void setChannelMode(ChannelMode mode)
    {
        switch(mode) {
            case ChannelMode::Default:
                setChannelSelection({});
                break;
            case ChannelMode::Mono:
                setChannelSelection(ChannelSelection{.mixMode = ChannelSelection::MixMode::MonoAverage});
                break;
        }
    }

    [[nodiscard]] uint64_t currentTimeMs() const;
    [[nodiscard]] bool getPcmWindow(PcmWindow& out, uint64_t centerTimeMs, uint64_t durationMs) const;
    [[nodiscard]] bool getPcmWindowEndingAt(PcmWindow& out, uint64_t endTimeMs, uint64_t durationMs) const;
    [[nodiscard]] bool getSpectrumWindow(SpectrumWindow& out, uint64_t centerTimeMs, int fftSize,
                                         SpectrumWindowFunction windowFunction = SpectrumWindowFunction::Hann) const;
    [[nodiscard]] bool getSpectrumWindowEndingAt(SpectrumWindow& out, uint64_t endTimeMs, int fftSize,
                                                 SpectrumWindowFunction windowFunction
                                                 = SpectrumWindowFunction::Hann) const;

private:
    friend class VisualisationService;

    VisualisationSession(std::shared_ptr<VisualisationBackend> backend, VisualisationService* service);

    std::unique_ptr<VisualisationSessionPrivate> p;
};

using VisualisationSessionPtr = std::shared_ptr<VisualisationSession>;

class FYCORE_EXPORT VisualisationService : public QObject
{
    Q_OBJECT

public:
    explicit VisualisationService(QObject* parent = nullptr);
    ~VisualisationService() override;

    //! Create a pull-based session for widget visualisation queries.
    [[nodiscard]] VisualisationSessionPtr createSession();

Q_SIGNALS:
    //! Emitted when session registration changes and engine analysis relays may need updating.
    void sessionActivityChanged();

private:
    friend class EngineHandler;

    [[nodiscard]] std::shared_ptr<VisualisationBackend> backend() const;
    [[nodiscard]] bool hasActiveSessions() const;

    std::unique_ptr<VisualisationServicePrivate> p;
};
} // namespace Fooyin
