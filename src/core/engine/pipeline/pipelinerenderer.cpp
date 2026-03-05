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

#include "pipelinerenderer.h"

#include "audioanalysisbus.h"
#include "core/engine/levelframe.h"
#include "dsp/dspregistry.h"

#include <core/engine/audioutils.h>
#include <utils/timeconstants.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <span>
#include <utility>

namespace {
void applyMasterVolume(Fooyin::ProcessingBufferList& chunks, double volume)
{
    const double gain = std::clamp(volume, 0.0, 1.0);
    if(gain == 1.0) {
        return;
    }

    for(size_t i{0}; i < chunks.count(); ++i) {
        auto* chunk = chunks.item(i);

        if(!chunk) {
            break;
        }

        auto& samples = chunk->samples();

        if(gain == 0.0) {
            std::ranges::fill(samples, 0.0);
        }
        else {
            for(double& sample : samples) {
                sample *= gain;
            }
        }
    }
}

uint64_t liveRevisionKey(const Fooyin::Engine::DspChainScope scope, uint64_t instanceId)
{
    static constexpr uint64_t ScopeBitMask = (1ULL << 63);
    const uint64_t scopeBit                = (scope == Fooyin::Engine::DspChainScope::PerTrack) ? ScopeBitMask : 0ULL;
    return scopeBit | (instanceId & ~ScopeBitMask);
}

std::vector<Fooyin::DspChain::NodeSnapshot> snapshotNodes(const std::vector<Fooyin::DspNodePtr>& nodes)
{
    std::vector<Fooyin::DspChain::NodeSnapshot> snapshot;
    snapshot.reserve(nodes.size());

    for(const auto& node : nodes) {
        Fooyin::DspChain::NodeSnapshot entry;
        if(node) {
            entry.id         = node->id();
            entry.name       = node->name();
            entry.enabled    = node->isEnabled();
            entry.instanceId = node->instanceId();
            entry.settings   = node->saveSettings();
        }
        snapshot.push_back(std::move(entry));
    }

    return snapshot;
}

std::vector<Fooyin::DspNodePtr> buildPerTrackNodesForDefs(const Fooyin::Engine::DspChain& defs,
                                                          Fooyin::DspRegistry* registry)
{
    if(!registry || defs.empty()) {
        return {};
    }

    std::vector<Fooyin::DspNodePtr> nodes;
    nodes.reserve(defs.size());

    for(const auto& entry : defs) {
        if(auto node = registry->create(entry.id)) {
            node->setInstanceId(entry.instanceId);
            node->setEnabled(entry.enabled);

            if(!entry.settings.isEmpty()) {
                node->loadSettings(entry.settings);
            }
            nodes.push_back(std::move(node));
        }
    }

    return nodes;
}
} // namespace

namespace Fooyin {
AudioFormat PipelineRenderer::configureForInputFormat(const AudioFormat& inputFormat, const AudioFormat& effectiveInput,
                                                      const SampleFormat outputBitdepth, size_t minProcessChunkFrames)
{
    m_inputFormat = inputFormat;

    m_dspChain.prepare(effectiveInput);
    m_outputFormat = m_dspChain.outputFormat();
    if(!m_outputFormat.isValid()) {
        m_outputFormat = effectiveInput;
    }

    if(outputBitdepth != SampleFormat::Unknown) {
        m_outputFormat.setSampleFormat(outputBitdepth);
    }

    m_mixer.setFormat(m_inputFormat);
    m_mixer.setOutputFormat(effectiveInput);
    setWorkingFormats(m_inputFormat, m_outputFormat);

    AudioFormat mixFormat{effectiveInput};
    mixFormat.setSampleFormat(SampleFormat::F64);

    m_processBuffer = ProcessingBuffer{mixFormat, 0};
    configureAnalysisScratch(mixFormat, minProcessChunkFrames);

    return m_outputFormat;
}

void PipelineRenderer::applyLiveSettingsUpdate(const Engine::LiveDspSettingsUpdate& update)
{
    if(update.instanceId == 0) {
        return;
    }

    const uint64_t key    = liveRevisionKey(update.scope, update.instanceId);
    const auto revisionIt = m_liveSettingsRevisionByKey.find(key);
    if(revisionIt != m_liveSettingsRevisionByKey.end() && update.revision <= revisionIt->second) {
        return;
    }
    m_liveSettingsRevisionByKey[key] = update.revision;

    if(update.scope == Engine::DspChainScope::PerTrack) {
        for(auto& entry : m_perTrackChainDefs) {
            if(entry.instanceId == update.instanceId) {
                entry.settings = update.settings;
            }
        }

        m_mixer.applyLivePerTrackDspSettings(update.instanceId, update.settings, update.revision);
        return;
    }

    for(size_t i{0}; i < m_dspChain.size(); ++i) {
        auto* node = m_dspChain.nodeAt(i);
        if(!node || node->instanceId() != update.instanceId || !node->supportsLiveSettings()) {
            continue;
        }

        node->loadSettings(update.settings);
    }
}

PipelineRenderer::RenderResult PipelineRenderer::render(int framesToProcess, OutputFader& outputFader,
                                                        bool outputSupportsVolume, double masterVolume,
                                                        AudioAnalysisBus* analysisBus, uint64_t playbackDelayMs)
{
    RenderResult result;

    if(framesToProcess <= 0) {
        return result;
    }

    const AudioFormat format = m_processBuffer.format();
    const int channels       = format.channelCount();
    if(!format.isValid() || channels <= 0) {
        return result;
    }

    const auto samplesToProcess = static_cast<size_t>(framesToProcess) * static_cast<size_t>(channels);
    m_processBuffer.resizeSamples(samplesToProcess);

    const auto mixerRead   = m_mixer.readWithStatus(m_processBuffer, framesToProcess);
    result.framesRead      = mixerRead.producedFrames;
    result.mixerBuffering  = mixerRead.buffering;
    result.primaryStreamId = mixerRead.primaryStreamId;

    if(result.framesRead <= 0) {
        return result;
    }

    if(result.framesRead != framesToProcess) {
        const auto samplesRead = static_cast<size_t>(result.framesRead) * static_cast<size_t>(channels);
        m_processBuffer.resizeSamples(samplesRead);
    }

    rebuildProcessChunksFromMixerRead(mixerRead, result.framesRead);
    m_dspChain.process(m_processChunks);

    outputFader.process(m_processChunks);

    if(!outputSupportsVolume) {
        applyMasterVolume(m_processChunks, masterVolume);
    }

    result.fadeCompletion = outputFader.takeCompletion();

    tapAnalysis(analysisBus, playbackDelayMs);

    return result;
}

void PipelineRenderer::clearRuntimeState()
{
    clearFormats();
    clearProcessingState();
}

void PipelineRenderer::clearProcessingState()
{
    m_processBuffer.reset();
    m_processChunks.clear();
    m_analysisScratch.clear();
}

void PipelineRenderer::configureAnalysisScratch(const AudioFormat& mixFormat, size_t minProcessChunkFrames)
{
    m_analysisScratch.clear();
    if(mixFormat.channelCount() <= 0) {
        return;
    }

    const size_t reserveSamples = static_cast<size_t>(mixFormat.channelCount()) * minProcessChunkFrames;
    m_analysisScratch.reserve(reserveSamples);
}

void PipelineRenderer::setWorkingFormats(const AudioFormat& input, const AudioFormat& output)
{
    m_inputFormat  = input;
    m_outputFormat = output;
}

void PipelineRenderer::clearFormats()
{
    m_inputFormat  = {};
    m_outputFormat = {};
}

void PipelineRenderer::resetLiveSettings()
{
    m_liveSettingsRevisionByKey.clear();
}

void PipelineRenderer::setOutputBitdepth(const SampleFormat bitdepth)
{
    if(bitdepth == SampleFormat::Unknown || !m_outputFormat.isValid()) {
        return;
    }

    m_outputFormat.setSampleFormat(bitdepth);
}

AudioFormat PipelineRenderer::predictMasterOutputFormat(const AudioFormat& input) const
{
    return m_dspChain.outputFormatFor(input);
}

int PipelineRenderer::masterLatencyFrames() const
{
    return std::max(0, m_dspChain.totalLatencyFrames());
}

double PipelineRenderer::totalLatencySeconds() const
{
    return std::max(0.0, m_dspChain.totalLatencySeconds() + m_mixer.primaryStreamDspLatencySeconds());
}

AudioFormat PipelineRenderer::processFormat() const
{
    return m_processBuffer.format();
}

const AudioFormat& PipelineRenderer::inputFormat() const
{
    return m_inputFormat;
}

const AudioFormat& PipelineRenderer::outputFormat() const
{
    return m_outputFormat;
}

void PipelineRenderer::setOutputFormat(const AudioFormat& outputFormat)
{
    m_outputFormat = outputFormat;
}

const Engine::DspChain& PipelineRenderer::perTrackChainDefs() const
{
    return m_perTrackChainDefs;
}

PipelineRenderer::PerTrackChainPatchPlan
PipelineRenderer::buildPerTrackChainPatchPlan(const Engine::DspChain& defs) const
{
    const size_t currentSize    = m_perTrackChainDefs.size();
    const size_t updatedSize    = defs.size();
    const size_t minCommonCount = std::min(currentSize, updatedSize);

    size_t prefixKeep{0};
    while(prefixKeep < minCommonCount && m_perTrackChainDefs[prefixKeep] == defs[prefixKeep]) {
        ++prefixKeep;
    }

    size_t suffixKeep{0};
    const size_t currentSuffixLimit = currentSize - prefixKeep;
    const size_t updatedSuffixLimit = updatedSize - prefixKeep;
    const size_t maxSuffix          = std::min(currentSuffixLimit, updatedSuffixLimit);
    while(suffixKeep < maxSuffix
          && m_perTrackChainDefs[(currentSize - 1) - suffixKeep] == defs[(updatedSize - 1) - suffixKeep]) {
        ++suffixKeep;
    }

    const size_t newMiddleStart = prefixKeep;
    const size_t newMiddleEnd   = updatedSize - suffixKeep;

    PerTrackChainPatchPlan plan;
    plan.prefixKeep     = prefixKeep;
    plan.oldMiddleCount = currentSize - prefixKeep - suffixKeep;
    plan.hasChanges     = (plan.oldMiddleCount > 0) || (newMiddleEnd > newMiddleStart);
    plan.newMiddleDefs.insert(plan.newMiddleDefs.end(), defs.begin() + static_cast<ptrdiff_t>(newMiddleStart),
                              defs.begin() + static_cast<ptrdiff_t>(newMiddleEnd));

    return plan;
}

void PipelineRenderer::setPerTrackChainDefs(const Engine::DspChain& defs)
{
    m_perTrackChainDefs = defs;
}

void PipelineRenderer::applyPerTrackChainPatch(const PerTrackChainPatchPlan& patchPlan, DspRegistry* registry,
                                               const bool preserveBufferedOutput)
{
    if(!patchPlan.hasChanges) {
        return;
    }

    const AudioMixer::PerTrackPatchPlan mixerPlan{
        .prefixKeep     = patchPlan.prefixKeep,
        .oldMiddleCount = patchPlan.oldMiddleCount,
        .newMiddleCount = patchPlan.newMiddleDefs.size(),
        .hasChanges     = patchPlan.hasChanges,
    };

    if(registry && m_mixer.canApplyIncrementalPerTrackPatch(mixerPlan)) {
        const bool patchApplied = m_mixer.applyIncrementalPerTrackPatch(
            mixerPlan,
            [registry, &patchPlan]() { return buildPerTrackNodesForDefs(patchPlan.newMiddleDefs, registry); },
            preserveBufferedOutput);
        if(patchApplied) {
            return;
        }
    }

    rebuildAllPerTrackDsps(registry, preserveBufferedOutput);
}

std::vector<DspNodePtr> PipelineRenderer::buildPerTrackNodes(DspRegistry* registry) const
{
    return buildPerTrackNodesForDefs(m_perTrackChainDefs, registry);
}

AudioFormat PipelineRenderer::predictPerTrackOutputFormat(const AudioFormat& inputFormat, DspRegistry* registry) const
{
    if(!registry || m_perTrackChainDefs.empty() || !inputFormat.isValid()) {
        return inputFormat;
    }

    AudioFormat current{inputFormat};

    for(const auto& entry : m_perTrackChainDefs) {
        if(auto node = registry->create(entry.id)) {
            node->setInstanceId(entry.instanceId);
            node->setEnabled(entry.enabled);

            if(!entry.settings.isEmpty()) {
                node->loadSettings(entry.settings);
            }

            if(node->isEnabled()) {
                current = node->outputFormat(current);
            }
        }
    }

    return current;
}

PipelineRenderer::MasterChainPatchPlan
PipelineRenderer::buildMasterChainPatchPlan(const std::vector<DspNodePtr>& nodes) const
{
    const auto currentSnapshot = m_dspChain.snapshot();
    const auto updatedSnapshot = snapshotNodes(nodes);

    const size_t currentSize    = currentSnapshot.size();
    const size_t updatedSize    = updatedSnapshot.size();
    const size_t minCommonCount = std::min(currentSize, updatedSize);

    size_t prefixKeep{0};
    while(prefixKeep < minCommonCount && currentSnapshot[prefixKeep] == updatedSnapshot[prefixKeep]) {
        ++prefixKeep;
    }

    size_t suffixKeep{0};
    const size_t currentSuffixLimit = currentSize - prefixKeep;
    const size_t updatedSuffixLimit = updatedSize - prefixKeep;
    const size_t maxSuffix          = std::min(currentSuffixLimit, updatedSuffixLimit);
    while(suffixKeep < maxSuffix
          && currentSnapshot[(currentSize - 1) - suffixKeep] == updatedSnapshot[(updatedSize - 1) - suffixKeep]) {
        ++suffixKeep;
    }

    MasterChainPatchPlan plan;
    plan.prefixKeep     = prefixKeep;
    plan.oldMiddleCount = currentSize - prefixKeep - suffixKeep;
    plan.newMiddleCount = updatedSize - prefixKeep - suffixKeep;
    plan.hasChanges     = (plan.oldMiddleCount > 0) || (plan.newMiddleCount > 0);

    return plan;
}

bool PipelineRenderer::canApplyIncrementalMasterPatch(const MasterChainPatchPlan& patchPlan,
                                                      const bool hasConfiguredInput) const
{
    if(!hasConfiguredInput || !patchPlan.hasChanges) {
        return false;
    }

    return patchPlan.prefixKeep <= m_dspChain.size()
        && patchPlan.oldMiddleCount <= (m_dspChain.size() - patchPlan.prefixKeep);
}

void PipelineRenderer::applyIncrementalMasterPatch(const MasterChainPatchPlan& patchPlan, std::vector<DspNodePtr> nodes)
{
    if(!patchPlan.hasChanges) {
        return;
    }

    const size_t replacementStart = patchPlan.prefixKeep;
    const size_t replacementEnd   = replacementStart + patchPlan.newMiddleCount;
    if(replacementEnd > nodes.size()) {
        replaceMasterNodes(std::move(nodes));
        return;
    }

    std::vector<DspNodePtr> replacementNodes;
    replacementNodes.reserve(patchPlan.newMiddleCount);

    for(size_t i{replacementStart}; i < replacementEnd; ++i) {
        replacementNodes.push_back(std::move(nodes[i]));
    }

    m_dspChain.replaceNodeRange(replacementStart, patchPlan.oldMiddleCount, std::move(replacementNodes));
}

void PipelineRenderer::replaceMasterNodes(std::vector<DspNodePtr> nodes)
{
    m_dspChain.replaceNodes(std::move(nodes));
}

void PipelineRenderer::reconfigureForChainChange(const AudioFormat& effectiveInput, const SampleFormat outputBitdepth,
                                                 bool hasConfiguredInput, size_t minProcessChunkFrames)
{
    if(hasConfiguredInput) {
        const bool masterInputChanged = !Audio::inputFormatsMatch(m_dspChain.inputFormat(), effectiveInput);
        if(masterInputChanged) {
            m_dspChain.prepare(effectiveInput);
        }

        m_outputFormat = m_dspChain.outputFormatFor(effectiveInput);

        if(outputBitdepth != SampleFormat::Unknown && m_outputFormat.isValid()) {
            m_outputFormat.setSampleFormat(outputBitdepth);
        }
    }

    m_mixer.setOutputFormat(effectiveInput);

    AudioFormat mixFormat{effectiveInput};
    mixFormat.setSampleFormat(SampleFormat::F64);

    m_processBuffer = ProcessingBuffer{mixFormat, 0};
    m_processChunks.clear();

    configureAnalysisScratch(mixFormat, minProcessChunkFrames);
}

void PipelineRenderer::resetMaster()
{
    m_dspChain.reset();
}

void PipelineRenderer::flushMaster(ProcessingBufferList& chunks, const DspNode::FlushMode mode)
{
    m_dspChain.flush(chunks, mode);
}

void PipelineRenderer::rebuildAllPerTrackDsps(DspRegistry* registry, const bool preserveBufferedOutput)
{
    m_mixer.rebuildAllPerTrackDsps([this, registry]() { return buildPerTrackNodes(registry); }, preserveBufferedOutput);
}

void PipelineRenderer::addTrackProcessorFactory(AudioMixer::TrackProcessorFactory factory)
{
    m_mixer.addTrackProcessorFactory(std::move(factory));
}

void PipelineRenderer::addStream(AudioStreamPtr stream, std::vector<DspNodePtr> perTrackNodes)
{
    m_mixer.addStream(std::move(stream), std::move(perTrackNodes));
}

void PipelineRenderer::removeStream(const StreamId id)
{
    m_mixer.removeStream(id);
}

void PipelineRenderer::switchTo(AudioStreamPtr stream, std::vector<DspNodePtr> perTrackNodes)
{
    m_mixer.switchTo(std::move(stream), std::move(perTrackNodes));
}

bool PipelineRenderer::hasStream(const StreamId id) const
{
    return m_mixer.hasStream(id);
}

AudioStreamPtr PipelineRenderer::primaryStream() const
{
    return m_mixer.primaryStream();
}

size_t PipelineRenderer::mixingStreamCount() const
{
    return m_mixer.mixingStreamCount();
}

void PipelineRenderer::resetStreamForSeek(const StreamId id)
{
    m_mixer.resetStreamForSeek(id);
}

void PipelineRenderer::resumeAll()
{
    m_mixer.resumeAll();
}

void PipelineRenderer::pauseAll()
{
    m_mixer.pauseAll();
}

void PipelineRenderer::clearAll()
{
    m_mixer.clear();
}

bool PipelineRenderer::needsMoreData() const
{
    return m_mixer.needsMoreData();
}

size_t PipelineRenderer::streamCount() const
{
    return m_mixer.streamCount();
}

BufferedDspStage::QueueResult PipelineRenderer::queueProcessedOutput(PipelineOutput& output, bool dither,
                                                                     const StreamId streamId, uint64_t epoch) const
{
    return output.queueProcessedOutput(m_processChunks, m_outputFormat, dither, streamId, epoch);
}

void PipelineRenderer::rebuildProcessChunksFromMixerRead(const AudioMixer::ReadResult& readResult, int framesRead)
{
    m_processChunks.clear();

    if(framesRead <= 0) {
        return;
    }

    const AudioFormat format = m_processBuffer.format();
    const int channels       = format.channelCount();

    if(!format.isValid() || channels <= 0) {
        m_processChunks.setToSingle(m_processBuffer);
        return;
    }

    const auto sourceSamples = m_processBuffer.constData();
    const size_t totalSamples
        = static_cast<size_t>(std::max(0, framesRead)) * static_cast<size_t>(std::max(1, channels));

    if(sourceSamples.size() < totalSamples) {
        m_processChunks.setToSingle(m_processBuffer);
        return;
    }

    if(readResult.primaryTimeline.empty()) {
        m_processChunks.setToSingle(m_processBuffer);
        return;
    }

    m_processChunks.reserve(readResult.primaryTimeline.size() + 1U);

    const auto addChunkFromSource
        = [&](int startFrame, int chunkFrames, uint64_t startNs, uint64_t sourceFrameDurationNs) -> bool {
        if(chunkFrames <= 0 || startFrame < 0) {
            return false;
        }

        const size_t sampleOffset = static_cast<size_t>(startFrame) * static_cast<size_t>(channels);
        const size_t sampleCount  = static_cast<size_t>(chunkFrames) * static_cast<size_t>(channels);

        if((sampleOffset + sampleCount) > sourceSamples.size()) {
            return false;
        }

        ProcessingBuffer chunk{
            sourceSamples.subspan(sampleOffset, sampleCount),
            format,
            startNs,
        };

        chunk.setSourceFrameDurationNs(sourceFrameDurationNs);
        m_processChunks.addChunk(std::move(chunk));

        return true;
    };

    const auto canMergeChunkMeta = [](uint64_t lhsStartNs, uint64_t lhsFrameDurationNs, int lhsFrames,
                                      uint64_t rhsStartNs, uint64_t rhsFrameDurationNs) -> bool {
        if(lhsFrames <= 0) {
            return false;
        }

        if(lhsFrameDurationNs == 0 || rhsFrameDurationNs == 0) {
            return lhsFrameDurationNs == 0 && rhsFrameDurationNs == 0;
        }

        if(lhsFrameDurationNs != rhsFrameDurationNs) {
            return false;
        }

        const uint64_t expectedStartNs
            = lhsStartNs + (lhsFrameDurationNs * static_cast<uint64_t>(std::max(0, lhsFrames)));
        return expectedStartNs == rhsStartNs;
    };

    int frameCursor{0};
    int pendingStartFrame{-1};
    int pendingFrames{0};
    uint64_t pendingStartNs{0};
    uint64_t pendingFrameDurationNs{0};

    const auto flushPendingChunk = [&]() -> bool {
        if(pendingStartFrame < 0 || pendingFrames <= 0) {
            return true;
        }

        if(!addChunkFromSource(pendingStartFrame, pendingFrames, pendingStartNs, pendingFrameDurationNs)) {
            return false;
        }

        pendingStartFrame      = -1;
        pendingFrames          = 0;
        pendingStartNs         = 0;
        pendingFrameDurationNs = 0;

        return true;
    };

    for(const auto& segment : readResult.primaryTimeline) {
        if(segment.frames <= 0 || frameCursor >= framesRead) {
            continue;
        }

        const int chunkFrames = std::min(segment.frames, framesRead - frameCursor);
        if(chunkFrames <= 0) {
            continue;
        }

        const uint64_t chunkStartNs         = segment.valid ? segment.startNs : 0;
        const uint64_t chunkFrameDurationNs = segment.valid ? segment.frameDurationNs : 0;

        if(pendingStartFrame < 0) {
            pendingStartFrame      = frameCursor;
            pendingFrames          = chunkFrames;
            pendingStartNs         = chunkStartNs;
            pendingFrameDurationNs = chunkFrameDurationNs;
        }
        else if(canMergeChunkMeta(pendingStartNs, pendingFrameDurationNs, pendingFrames, chunkStartNs,
                                  chunkFrameDurationNs)) {
            pendingFrames += chunkFrames;
        }
        else {
            if(!flushPendingChunk()) {
                m_processChunks.setToSingle(m_processBuffer);
                return;
            }

            pendingStartFrame      = frameCursor;
            pendingFrames          = chunkFrames;
            pendingStartNs         = chunkStartNs;
            pendingFrameDurationNs = chunkFrameDurationNs;
        }

        frameCursor += chunkFrames;
    }

    if(frameCursor < framesRead) {
        const int remainingFrames = framesRead - frameCursor;
        if(remainingFrames > 0 && pendingStartFrame >= 0 && pendingFrameDurationNs == 0) {
            pendingFrames += remainingFrames;
        }
        else {
            if(!flushPendingChunk()) {
                m_processChunks.setToSingle(m_processBuffer);
                return;
            }

            pendingStartFrame      = frameCursor;
            pendingFrames          = remainingFrames;
            pendingStartNs         = 0;
            pendingFrameDurationNs = 0;
        }
    }

    if(!flushPendingChunk()) {
        m_processChunks.setToSingle(m_processBuffer);
        return;
    }

    if(m_processChunks.count() == 0) {
        m_processChunks.setToSingle(m_processBuffer);
    }
}

void PipelineRenderer::tapAnalysis(AudioAnalysisBus* analysisBus, uint64_t playbackDelayMs)
{
    if(!analysisBus || !analysisBus->hasSubscription(Engine::AnalysisDataType::LevelFrameData)) {
        return;
    }

    if(m_processChunks.count() == 0) {
        return;
    }

    int channelCount{0};
    int sampleRate{0};
    uint64_t streamTimeMs{0};
    bool haveTiming{false};
    size_t totalSamples{0};

    for(size_t i{0}; i < m_processChunks.count(); ++i) {
        const auto* chunk = m_processChunks.item(i);
        if(!chunk || !chunk->isValid() || chunk->sampleCount() <= 0) {
            continue;
        }

        const auto format = chunk->format();
        if(!format.isValid() || format.sampleFormat() != SampleFormat::F64 || format.channelCount() <= 0
           || format.channelCount() > LevelFrame::MaxChannels || format.sampleRate() <= 0) {
            continue;
        }

        if(!haveTiming) {
            channelCount = format.channelCount();
            sampleRate   = format.sampleRate();
            streamTimeMs = chunk->startTimeNs() / Time::NsPerMs;
            haveTiming   = true;
        }
        else if(format.channelCount() != channelCount || format.sampleRate() != sampleRate) {
            return;
        }

        totalSamples += static_cast<size_t>(chunk->sampleCount());
    }

    if(!haveTiming || totalSamples == 0) {
        return;
    }

    if(m_analysisScratch.size() < totalSamples) {
        m_analysisScratch.resize(totalSamples);
    }

    size_t writeOffset{0};

    for(size_t i{0}; i < m_processChunks.count(); ++i) {
        const auto* chunk = m_processChunks.item(i);
        if(!chunk || !chunk->isValid() || chunk->sampleCount() <= 0) {
            continue;
        }

        const auto format = chunk->format();
        if(!format.isValid() || format.sampleFormat() != SampleFormat::F64 || format.channelCount() != channelCount
           || format.sampleRate() != sampleRate) {
            continue;
        }

        const auto samples = chunk->constData();
        std::ranges::transform(samples, m_analysisScratch.begin() + static_cast<std::ptrdiff_t>(writeOffset),
                               [](double sample) { return static_cast<float>(sample); });
        writeOffset += samples.size();
    }

    if(writeOffset == 0) {
        return;
    }

    const auto presentationTime = AudioAnalysisBus::Clock::now() + std::chrono::milliseconds{playbackDelayMs};
    analysisBus->push(std::span<const float>{m_analysisScratch.data(), writeOffset}, channelCount, sampleRate,
                      streamTimeMs, presentationTime);
}
} // namespace Fooyin
