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

#include "mixer/audiomixer.h"
#include "pipelineoutput.h"

#include "core/engine/dsp/dspchain.h"
#include "core/engine/output/outputfader.h"

#include <cstddef>
#include <optional>
#include <unordered_map>
#include <vector>

namespace Fooyin {
class AudioAnalysisBus;
class DspRegistry;

/*!
 * Render unit that bridges mixer output to master DSP/output-fader processing.
 *
 * Responsibilities:
 * - configure working input/output/process formats,
 * - run `AudioMixer` reads for a target frame window,
 * - apply master DSP chain and output fader,
 * - expose render metadata (primary stream id, fade completion),
 * - provide predicted formats/latency for control-path decisions.
 */
class PipelineRenderer
{
public:
    struct MasterChainPatchPlan
    {
        size_t prefixKeep{0};
        size_t oldMiddleCount{0};
        size_t newMiddleCount{0};
        bool hasChanges{false};
    };

    struct PerTrackChainPatchPlan
    {
        size_t prefixKeep{0};
        size_t oldMiddleCount{0};
        Engine::DspChain newMiddleDefs;
        bool hasChanges{false};
    };

    struct RenderResult
    {
        int framesRead{0};
        bool mixerBuffering{false};
        StreamId primaryStreamId{InvalidStreamId};
        //! Optional fade completion emitted during master fader processing.
        std::optional<OutputFader::Completion> fadeCompletion;
    };

    [[nodiscard]] AudioFormat configureForInputFormat(const AudioFormat& inputFormat, const AudioFormat& effectiveInput,
                                                      SampleFormat outputBitdepth, size_t minProcessChunkFrames);

    void applyLiveSettingsUpdate(const Engine::LiveDspSettingsUpdate& update);

    //! Render one cycle worth of frames through mixer + master chain + fader.
    [[nodiscard]] RenderResult render(int framesToProcess, OutputFader& outputFader, bool outputSupportsVolume,
                                      double masterVolume, AudioAnalysisBus* analysisBus, uint64_t playbackDelayMs);

    //! Clear runtime-only processing state (buffers, temporary chunks).
    void clearRuntimeState();
    //! Clear master/mixer processing state while preserving configured chain defs.
    void clearProcessingState();
    //! Drop cached live-settings revision map.
    void resetLiveSettings();

    void setOutputBitdepth(SampleFormat bitdepth);

    [[nodiscard]] AudioFormat predictMasterOutputFormat(const AudioFormat& input) const;
    [[nodiscard]] int masterLatencyFrames() const;
    [[nodiscard]] double totalLatencySeconds() const;
    [[nodiscard]] AudioFormat processFormat() const;
    [[nodiscard]] const AudioFormat& inputFormat() const;

    [[nodiscard]] const AudioFormat& outputFormat() const;
    void setOutputFormat(const AudioFormat& outputFormat);

    [[nodiscard]] const Engine::DspChain& perTrackChainDefs() const;
    [[nodiscard]] PerTrackChainPatchPlan buildPerTrackChainPatchPlan(const Engine::DspChain& defs) const;
    void setPerTrackChainDefs(const Engine::DspChain& defs);
    void applyPerTrackChainPatch(const PerTrackChainPatchPlan& patchPlan, DspRegistry* registry,
                                 bool preserveBufferedOutput = false);
    [[nodiscard]] std::vector<DspNodePtr> buildPerTrackNodes(DspRegistry* registry) const;
    [[nodiscard]] AudioFormat predictPerTrackOutputFormat(const AudioFormat& inputFormat, DspRegistry* registry) const;
    [[nodiscard]] MasterChainPatchPlan buildMasterChainPatchPlan(const std::vector<DspNodePtr>& nodes) const;
    [[nodiscard]] bool canApplyIncrementalMasterPatch(const MasterChainPatchPlan& patchPlan,
                                                      bool hasConfiguredInput) const;
    void applyIncrementalMasterPatch(const MasterChainPatchPlan& patchPlan, std::vector<DspNodePtr> nodes);
    void replaceMasterNodes(std::vector<DspNodePtr> nodes);
    void reconfigureForChainChange(const AudioFormat& effectiveInput, SampleFormat outputBitdepth,
                                   bool hasConfiguredInput, size_t minProcessChunkFrames);
    void resetMaster();
    void flushMaster(ProcessingBufferList& chunks, DspNode::FlushMode mode);
    void rebuildAllPerTrackDsps(DspRegistry* registry, bool preserveBufferedOutput = false);

    void addTrackProcessorFactory(AudioMixer::TrackProcessorFactory factory);

    void addStream(AudioStreamPtr stream, std::vector<DspNodePtr> perTrackNodes);
    void removeStream(StreamId id);
    void switchTo(AudioStreamPtr stream, std::vector<DspNodePtr> perTrackNodes);

    [[nodiscard]] bool hasStream(StreamId id) const;
    [[nodiscard]] AudioStreamPtr primaryStream() const;
    [[nodiscard]] size_t mixingStreamCount() const;

    void resetStreamForSeek(StreamId id);
    void resumeAll();
    void pauseAll();
    void clearAll();

    [[nodiscard]] bool needsMoreData() const;
    [[nodiscard]] size_t streamCount() const;

    [[nodiscard]] BufferedDspStage::QueueResult queueProcessedOutput(PipelineOutput& output, bool dither,
                                                                     StreamId streamId, uint64_t epoch) const;

private:
    void configureAnalysisScratch(const AudioFormat& mixFormat, size_t minProcessChunkFrames);
    void setWorkingFormats(const AudioFormat& input, const AudioFormat& output);
    void clearFormats();
    void rebuildProcessChunksFromMixerRead(const AudioMixer::ReadResult& readResult, int framesRead);
    void tapAnalysis(AudioAnalysisBus* analysisBus, uint64_t playbackDelayMs);

    Engine::DspChain m_perTrackChainDefs;

    std::unordered_map<uint64_t, uint64_t> m_liveSettingsRevisionByKey;

    ProcessingBufferList m_processChunks;
    std::vector<float> m_analysisScratch;
    ProcessingBuffer m_processBuffer;

    DspChain m_dspChain;
    AudioMixer m_mixer;

    AudioFormat m_inputFormat;
    AudioFormat m_outputFormat;
};
} // namespace Fooyin
