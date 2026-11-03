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

#include <core/engine/conversion/conversionrunner.h>

#include "audioutils.h"
#include "output/postprocessor/replaygainprocessor.h"
#include "outputtransaction.h"

#include <core/engine/audioconverter.h>
#include <core/engine/dsp/dspchain.h>
#include <core/engine/dsp/dspregistry.h>
#include <core/engine/dsp/processingbuffer.h>
#include <core/engine/dsp/processingbufferlist.h>
#include <core/engine/verification/audioverifier.h>
#include <utils/scopeguard.h>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QSaveFile>

#include <array>
#include <atomic>
#include <expected>
#include <future>
#include <limits>
#include <mutex>
#include <optional>
#include <set>
#include <span>
#include <thread>
#include <vector>

using namespace Qt::StringLiterals;

constexpr size_t TargetReadBytes = 256UL * 1024;

namespace Fooyin {
namespace {
bool shouldCancel(const ConversionRunner::Request& request)
{
    return request.cancelCallback && request.cancelCallback();
}

void reportProgress(const ConversionRunner::Request& request, int index, const Track& track, const QString& outputPath,
                    uint64_t positionMs, bool finished = false)
{
    if(!request.progressCallback) {
        return;
    }

    request.progressCallback({
        .trackIndex       = index,
        .trackCount       = static_cast<int>(request.job.tracks.size()),
        .sourcePath       = track.filepath(),
        .outputPath       = outputPath,
        .sourcePositionMs = positionMs,
        .sourceDurationMs = track.duration(),
        .finished         = finished,
    });
}

ConversionTrackResult failedResult(const Track& track, const QString& outputPath, QString error)
{
    return {
        .sourceTrack = track,
        .outputPath  = outputPath,
        .status      = ConversionResultStatus::Failed,
        .error       = std::move(error),
        .warnings    = {},
    };
}

bool profileSupportsPictures(const AudioEncoderRegistry& registry, const QString& profileId)
{
    const auto info = registry.encoderInfo(profileId);
    return info && info->supportsPictures;
}

TrackCovers readCovers(const AudioLoader& loader, const Track& track)
{
    constexpr std::array coverTypes{
        Track::Cover::Front,
        Track::Cover::Back,
        Track::Cover::Artist,
        Track::Cover::Other,
    };

    static const QMimeDatabase mimeDb;
    TrackCovers covers;

    for(const Track::Cover type : coverTypes) {
        QByteArray data = loader.readTrackCover(track, type);
        if(data.isEmpty()) {
            continue;
        }
        covers.emplace(type, CoverImage{.mimeType = mimeDb.mimeTypeForData(data).name(), .data = std::move(data)});
    }

    return covers;
}

QStringList copySidecarFiles(const QString& pattern, const TrackList& tracks, const QString& outputPath,
                             std::set<QString>& handledCopies)
{
    QStringList filters;

    const auto parts = pattern.split(u';', Qt::SkipEmptyParts);
    for(QString filter : parts) {
        filter = filter.trimmed();
        if(!filter.isEmpty()) {
            filters.push_back(std::move(filter));
        }
    }

    if(filters.isEmpty()) {
        return {};
    }

    QStringList warnings;
    std::set<QString> sourceFolders;

    const QFileInfo outputInfo{outputPath};
    const QDir destination = outputInfo.absoluteDir();

    for(const Track& track : tracks) {
        const QDir source        = QFileInfo{track.filepath()}.absoluteDir();
        const QString sourcePath = source.absolutePath();
        if(sourceFolders.contains(sourcePath)) {
            continue;
        }
        sourceFolders.emplace(sourcePath);

        const QFileInfoList files
            = source.entryInfoList(filters, QDir::Files | QDir::Readable | QDir::NoDotAndDotDot, QDir::Name);
        for(const QFileInfo& file : files) {
            const QString sourceFile      = file.absoluteFilePath();
            const QString destinationFile = destination.filePath(file.fileName());
            if(sourceFile == outputInfo.absoluteFilePath() || sourceFile == destinationFile) {
                continue;
            }

            const QString copyKey = sourceFile + u'\n' + destinationFile;
            if(handledCopies.contains(copyKey)) {
                continue;
            }
            handledCopies.insert(copyKey);

            if(QFileInfo::exists(destinationFile)) {
                warnings.push_back(u"Sidecar file already exists: %1"_s.arg(file.fileName()));
                continue;
            }
            if(!QFile::copy(sourceFile, destinationFile)) {
                warnings.push_back(u"Failed to copy sidecar file: %1"_s.arg(file.fileName()));
            }
        }
    }

    return warnings;
}

QString verifyOutput(const ConversionRunner::Request& request, const QString& outputPath)
{
    const auto results = AudioVerifier::run({.audioLoader      = request.audioLoader,
                                             .tracks           = {Track{outputPath}},
                                             .verifyIntegrity  = true,
                                             .progressCallback = {},
                                             .cancelCallback   = request.cancelCallback,
                                             .observer         = {}});
    if(results.empty()) {
        return u"Converted output could not be opened"_s;
    }

    const AudioVerificationResult& result = results.front();
    if(result.status == AudioVerificationStatus::Succeeded && result.decodedFrames > 0) {
        return {};
    }
    return !result.error.isEmpty() ? result.error : u"Converted output contains no decodable audio"_s;
}

struct PcmHashResult
{
    bool ok{false};
    AudioFormat format;
    QByteArray hash;
    uint64_t frames{0};
    QString error;
};

uint64_t framesForDuration(uint64_t durationMs, int sampleRate)
{
    const uint64_t secs = durationMs / 1000;
    const uint64_t rem  = durationMs % 1000;

    const auto sRate = static_cast<uint64_t>(sampleRate);

    if(secs > static_cast<uint64_t>(std::numeric_limits<int>::max()) / sRate) {
        return std::numeric_limits<int>::max();
    }

    const uint64_t frames = (secs * sRate) + ((rem * sRate) / 1000);

    return static_cast<int>(frames);
}

AudioBuffer trimBuffer(const AudioBuffer& buffer, int frames)
{
    if(!buffer.isValid() || frames < 0 || frames >= buffer.frameCount()) {
        return buffer;
    }

    const auto bytes = static_cast<size_t>(buffer.format().bytesForFrames(frames));
    return {buffer.constData().first(bytes), buffer.format(), buffer.startTime()};
}

PcmHashResult pcmHashFailure(QString error)
{
    PcmHashResult result;
    result.error = std::move(error);
    return result;
}

PcmHashResult calculatePcmHash(const ConversionRunner::Request& request, const Track& track,
                               std::optional<AudioFormat> comparisonFormat, SampleFormat sampleFormat)
{
    auto loaded
        = request.audioLoader->loadDecoderForTrack(track, AudioDecoder::NoLooping | AudioDecoder::ForConversion);
    if(!loaded.decoder || !loaded.format) {
        return pcmHashFailure(u"PCM verification could not open input"_s);
    }

    AudioFormat format = comparisonFormat.value_or(*loaded.format);
    if(!comparisonFormat && sampleFormat != SampleFormat::Unknown) {
        format.setSampleFormat(sampleFormat);
    }

    if(loaded.format->sampleRate() != format.sampleRate() || loaded.format->channelCount() != format.channelCount()) {
        return pcmHashFailure(u"PCM verification format mismatch"_s);
    }

    loaded.decoder->start();
    const auto stopDecoder = scopeGuard([&loaded] { loaded.decoder->stop(); });

    if(track.offset() > 0) {
        if(!loaded.decoder->isSeekable()) {
            return pcmHashFailure(u"PCM verification cannot seek to the track segment"_s);
        }
        loaded.decoder->seek(track.offset());
    }

    std::optional<uint64_t> framesRemaining;
    if(track.isBoundedSegment() && track.duration() > 0) {
        const uint64_t duration = track.duration();
        framesRemaining         = framesForDuration(duration, loaded.format->sampleRate());
    }

    QCryptographicHash hash{QCryptographicHash::Sha256};
    uint64_t frames{0};

    while(true) {
        if(shouldCancel(request)) {
            return pcmHashFailure(u"Conversion cancelled"_s);
        }
        auto result = loaded.decoder->readAudio(TargetReadBytes);
        if(result.status == AudioDecoder::ReadStatus::NeedMoreInput) {
            continue;
        }
        if(result.status == AudioDecoder::ReadStatus::EndOfStream) {
            break;
        }
        if(result.status == AudioDecoder::ReadStatus::Error) {
            return pcmHashFailure(!result.error.isEmpty() ? result.error : u"PCM verification decode failed"_s);
        }

        AudioBuffer buffer{result.buffer};

        if(framesRemaining) {
            const int count
                = static_cast<int>(std::min<uint64_t>(*framesRemaining, static_cast<uint64_t>(buffer.frameCount())));
            buffer = trimBuffer(buffer, count);
            *framesRemaining -= static_cast<uint64_t>(count);
        }

        const AudioBuffer converted = buffer.format() == format ? buffer : Audio::convert(buffer, format);
        if(!converted.isValid()) {
            return pcmHashFailure(u"PCM verification conversion failed"_s);
        }

        const auto bytes = converted.constData();
        hash.addData(QByteArrayView{reinterpret_cast<const char*>(bytes.data()), static_cast<qsizetype>(bytes.size())});
        frames += static_cast<uint64_t>(converted.frameCount());

        if(framesRemaining && *framesRemaining == 0) {
            break;
        }
    }

    PcmHashResult result;
    result.ok     = true;
    result.format = format;
    result.hash   = hash.result();
    result.frames = frames;
    return result;
}

bool supportsLosslessPcmVerification(const ConversionPreset& preset)
{
    const auto& processing = preset.processing;
    const auto& profile    = preset.encoder.profile;
    const bool lossless    = profile.mode == EncoderMode::LosslessCompression
                          || profile.codecName.startsWith(u"pcm_"_s, Qt::CaseInsensitive);
    return lossless && preset.encoder.ditherMode == DitherMode::Never && !preset.preview.enabled
        && processing.replayGainMode == ConversionReplayGainMode::None && processing.dspChain.empty();
}

QString verifyLosslessPcm(const ConversionRunner::Request& request, const Track& source, const QString& outputPath,
                          const ConversionPreset& preset)
{
    const PcmHashResult expected = calculatePcmHash(request, source, {}, preset.encoder.outputSampleFormat);
    if(!expected.ok) {
        return expected.error;
    }

    const PcmHashResult actual = calculatePcmHash(request, Track{outputPath}, expected.format, SampleFormat::Unknown);
    if(!actual.ok) {
        return actual.error;
    }

    if(expected.frames != actual.frames || expected.hash != actual.hash) {
        return u"Lossless PCM verification failed"_s;
    }

    return {};
}

std::unique_ptr<ReplayGainProcessor> createReplayGainProcessor(const ConversionProcessing& processing,
                                                               const Track& track, const AudioFormat& format)
{
    if(processing.replayGainMode == ConversionReplayGainMode::None) {
        return {};
    }

    auto sharedSettings = ReplayGainProcessor::makeSharedSettings();
    sharedSettings->update([&processing](ReplayGainProcessor::RuntimeSettings& settings) {
        settings.mode       = processing.replayGainMode == ConversionReplayGainMode::Track
                                ? ReplayGainProcessor::SelectionMode::Track
                                : ReplayGainProcessor::SelectionMode::Album;
        settings.processing = Engine::RGProcessing{Engine::ApplyGain};
        if(processing.replayGainPreventClipping) {
            settings.processing |= Engine::PreventClipping;
        }
        settings.rgPreampDb    = processing.replayGainPreampDb;
        settings.nonRgPreampDb = processing.replayGainWithoutInfoPreampDb;
    });

    auto processor = std::make_unique<ReplayGainProcessor>(
        std::shared_ptr<const ReplayGainProcessor::SharedSettings>{std::move(sharedSettings)});
    processor->init(track, format);

    return processor;
}

bool populateDspChain(const Engine::DspChain& definitions, const DspRegistry* registry, DspChain& chain, QString& error)
{
    if(definitions.empty()) {
        return true;
    }

    for(const auto& definition : definitions) {
        auto node = registry->create(definition.id);
        if(!node) {
            if(!definition.enabled) {
                continue;
            }
            error = u"DSP is unavailable: %1"_s.arg(definition.name.isEmpty() ? definition.id : definition.name);
            return false;
        }

        node->setEnabled(definition.enabled);
        node->setInstanceId(definition.instanceId);

        if(!definition.settings.isEmpty() && !node->loadSettings(definition.settings)) {
            error
                = u"Failed to load DSP settings: %1"_s.arg(definition.name.isEmpty() ? definition.id : definition.name);
            return false;
        }

        chain.addNode(std::move(node));
    }

    return true;
}

AudioEncoderSettings encoderSettingsForTrack(const AudioEncoderSettings& settings, const Track& track,
                                             const AudioFormat& inputFormat)
{
    AudioEncoderSettings resolved{settings};
    if(resolved.ditherMode == DitherMode::LossySourceOnly) {
        resolved.ditherMode
            = track.encoding().compare(u"Lossy"_s, Qt::CaseInsensitive) == 0 ? DitherMode::Always : DitherMode::Never;
    }
    else if(resolved.ditherMode == DitherMode::Automatic && resolved.outputSampleFormat != SampleFormat::Unknown) {
        resolved.ditherMode = Audio::shouldAutomaticallyDither(inputFormat.sampleFormat(), resolved.outputSampleFormat)
                                ? DitherMode::Always
                                : DitherMode::Never;
    }
    return resolved;
}

std::expected<uint64_t, QString> previewDurationMs(const ConversionPreset& preset, const Track& track)
{
    if(preset.preview.lengthPercentage <= 0 || preset.preview.lengthPercentage > 100) {
        return std::unexpected{u"Preview length percentage must be between 1 and 100"_s};
    }

    if(track.duration() == 0) {
        return std::unexpected{u"Preview generation requires a known track duration"_s};
    }

    const auto percentage   = static_cast<uint64_t>(preset.preview.lengthPercentage);
    const uint64_t duration = ((track.duration() / 100) * percentage) + ((track.duration() % 100) * percentage / 100);
    return std::max<uint64_t>(1, duration);
}

ProcessingBuffer makeProcessingBuffer(const AudioBuffer& input, ReplayGainProcessor* replayGain,
                                      const AudioFormat& processingFormat)
{
    const AudioBuffer converted = Audio::convert(input, processingFormat);
    if(!converted.isValid()) {
        return {};
    }

    std::vector<double> samples(static_cast<size_t>(converted.sampleCount()));
    std::memcpy(samples.data(), converted.data(), static_cast<size_t>(converted.byteCount()));

    if(replayGain) {
        replayGain->process(samples.data(), samples.size());
    }

    ProcessingBuffer result{samples, processingFormat, converted.startTime() * 1'000'000ULL};
    if(processingFormat.sampleRate() > 0) {
        result.setSourceFrameDurationNs(1'000'000'000ULL / static_cast<uint64_t>(processingFormat.sampleRate()));
    }
    return result;
}

AudioEncoder::Result writeProcessingChunks(AudioEncoder& encoder, const ProcessingBufferList& chunks)
{
    for(size_t i{0}; i < chunks.count(); ++i) {
        const auto* chunk = chunks.item(i);
        if(!chunk || !chunk->isValid()) {
            continue;
        }
        auto result = encoder.write(chunk->toAudioBuffer());
        if(!result.ok) {
            return result;
        }
    }
    return AudioEncoder::Result::success();
}

struct TrackEncodingResult
{
    bool ok{false};
    bool cancelled{false};
    AudioFormat encoderInputFormat;
    QString error;
    QStringList warnings;
};

struct SharedProcessingContext
{
    std::unique_ptr<DspChain> dspChain;
    AudioFormat processingInputFormat;
    AudioFormat encoderInputFormat;
    bool prepared{false};
};

TrackEncodingResult trackEncodingFailure(QString error, const AudioFormat& format = {})
{
    return {
        .ok                 = false,
        .cancelled          = false,
        .encoderInputFormat = format,
        .error              = std::move(error),
        .warnings           = {},
    };
}

TrackEncodingResult encodeTrack(const ConversionRunner::Request& request, const Track& track, int index,
                                const QString& outputPath, const QString& encoderOutputPath, AudioEncoder& encoder,
                                bool initialiseEncoder, const AudioFormat& requiredFormat,
                                SharedProcessingContext* sharedProcessing = nullptr, bool flushProcessingAtEnd = true)
{
    std::optional<uint64_t> previewDuration;
    if(request.job.preset.preview.enabled) {
        const auto duration = previewDurationMs(request.job.preset, track);
        if(!duration) {
            return trackEncodingFailure(duration.error());
        }
        previewDuration = *duration;
    }

    auto loaded
        = request.audioLoader->loadDecoderForTrack(track, AudioDecoder::NoLooping | AudioDecoder::ForConversion);
    if(!loaded.decoder || !loaded.format) {
        return trackEncodingFailure(u"No decoder available"_s);
    }

    loaded.decoder->start();

    auto finishDecoder = scopeGuard([&loaded] {
        if(loaded.decoder) {
            loaded.decoder->stop();
        }
    });

    if(track.offset() > 0) {
        if(!loaded.decoder->isSeekable()) {
            return trackEncodingFailure(u"Decoder cannot seek to the track segment"_s);
        }
        loaded.decoder->seek(track.offset());
    }

    reportProgress(request, index, track, outputPath, 0);

    const bool hasReplayGain = request.job.preset.processing.replayGainMode != ConversionReplayGainMode::None;
    const bool hasDsp        = !request.job.preset.processing.dspChain.empty();
    const bool hasProcessing = hasReplayGain || hasDsp;

    std::optional<uint64_t> sourceDuration;
    if(track.isBoundedSegment() && track.duration() > 0) {
        sourceDuration = track.duration();
    }
    if(previewDuration) {
        sourceDuration = sourceDuration ? std::min(*sourceDuration, *previewDuration) : previewDuration;
    }
    if(sourceDuration && *sourceDuration == 0) {
        return trackEncodingFailure(u"Track contains no audio after CUE pregap removal"_s);
    }

    uint64_t sourceFramesRemaining
        = sourceDuration ? framesForDuration(*sourceDuration, loaded.format->sampleRate()) : 0;

    AudioFormat processingInputFormat{*loaded.format};
    if(hasProcessing) {
        processingInputFormat.setSampleFormat(SampleFormat::F64);
    }

    auto replayGain = createReplayGainProcessor(request.job.preset.processing, track, processingInputFormat);

    std::unique_ptr<DspChain> localDspChain;
    DspChain* dspChain{nullptr};
    QString dspError;
    AudioFormat encoderInputFormat;

    if(sharedProcessing && hasDsp) {
        if(!sharedProcessing->prepared) {
            sharedProcessing->processingInputFormat = processingInputFormat;
            sharedProcessing->dspChain              = std::make_unique<DspChain>();
            if(!populateDspChain(request.job.preset.processing.dspChain, request.dspRegistry,
                                 *sharedProcessing->dspChain, dspError)) {
                return trackEncodingFailure(dspError);
            }

            sharedProcessing->dspChain->prepare(processingInputFormat);

            if(!sharedProcessing->dspChain->outputFormat().isValid()) {
                return trackEncodingFailure(u"DSP chain produced an invalid format"_s);
            }

            encoderInputFormat
                = sharedProcessing->dspChain ? sharedProcessing->dspChain->outputFormat() : processingInputFormat;
            sharedProcessing->encoderInputFormat = encoderInputFormat;
            sharedProcessing->prepared           = true;
        }
        else {
            if(processingInputFormat != sharedProcessing->processingInputFormat) {
                return trackEncodingFailure(u"Continuous DSP requires matching source processing formats"_s,
                                            sharedProcessing->encoderInputFormat);
            }
            encoderInputFormat = sharedProcessing->encoderInputFormat;
        }

        dspChain = sharedProcessing->dspChain.get();
    }
    else {
        if(hasDsp) {
            localDspChain = std::make_unique<DspChain>();
            if(!populateDspChain(request.job.preset.processing.dspChain, request.dspRegistry, *localDspChain,
                                 dspError)) {
                return trackEncodingFailure(dspError);
            }

            localDspChain->prepare(processingInputFormat);

            if(!localDspChain->outputFormat().isValid()) {
                return trackEncodingFailure(u"DSP chain produced an invalid format"_s);
            }

            dspChain = localDspChain.get();
        }

        encoderInputFormat = dspChain ? dspChain->outputFormat() : processingInputFormat;
    }

    if(requiredFormat.isValid() && encoderInputFormat != requiredFormat) {
        return trackEncodingFailure(u"Merged tracks must have matching processed audio formats"_s, encoderInputFormat);
    }

    if(initialiseEncoder) {
        const AudioEncoderSettings encoderSettings
            = encoderSettingsForTrack(request.job.preset.encoder, track, encoderInputFormat);
        const auto result = encoder.init(encoderOutputPath, encoderInputFormat, encoderSettings);
        if(!result.ok) {
            return trackEncodingFailure(result.error, encoderInputFormat);
        }
    }

    bool failed{false};
    bool sourceComplete{false};
    QString error;
    AudioEncoder::Result encoderResult;

    if(request.sourceObserver) {
        request.sourceObserver->trackStarted(track, *loaded.format);
    }

    const auto observerGuard = scopeGuard([&request, &track, &failed, &sourceComplete]() {
        if(request.sourceObserver) {
            request.sourceObserver->trackFinished(track, !failed && sourceComplete);
        }
    });

    while(true) {
        if(shouldCancel(request)) {
            const QStringList warnings = loaded.decoder->takeWarnings();
            return {
                .ok                 = false,
                .cancelled          = true,
                .encoderInputFormat = encoderInputFormat,
                .error              = u"Conversion cancelled"_s,
                .warnings           = warnings,
            };
        }

        auto readResult = loaded.decoder->readAudio(TargetReadBytes);
        switch(readResult.status) {
            case AudioDecoder::ReadStatus::DecodedAudio: {
                AudioBuffer inputBuffer{readResult.buffer};
                if(sourceDuration) {
                    const int frames = static_cast<int>(
                        std::min<uint64_t>(sourceFramesRemaining, static_cast<uint64_t>(inputBuffer.frameCount())));
                    inputBuffer = trimBuffer(inputBuffer, frames);
                    sourceFramesRemaining -= static_cast<uint64_t>(frames);
                    sourceComplete = sourceFramesRemaining == 0;
                }

                if(request.sourceObserver && inputBuffer.frameCount() > 0) {
                    request.sourceObserver->sourceAudio(track, inputBuffer);
                }

                if(hasProcessing) {
                    ProcessingBuffer processed
                        = makeProcessingBuffer(inputBuffer, replayGain.get(), processingInputFormat);
                    if(!processed.isValid()) {
                        failed = true;
                        error  = u"Failed to prepare audio for conversion processing"_s;
                        break;
                    }

                    ProcessingBufferList chunks;
                    chunks.addChunk(std::move(processed));
                    if(dspChain) {
                        dspChain->process(chunks);
                    }
                    encoderResult = writeProcessingChunks(encoder, chunks);
                }
                else {
                    encoderResult = encoder.write(inputBuffer);
                }

                if(!encoderResult.ok) {
                    failed = true;
                    error  = encoderResult.error;
                }
                else {
                    reportProgress(request, index, track, outputPath, inputBuffer.endTime());
                }

                break;
            }
            case AudioDecoder::ReadStatus::NeedMoreInput:
                continue;
            case AudioDecoder::ReadStatus::EndOfStream:
                sourceComplete = true;
                break;
            case AudioDecoder::ReadStatus::Error:
                failed = true;
                error  = !readResult.error.isEmpty() ? readResult.error : u"Decoder error"_s;
                break;
        }

        if(failed || sourceComplete || readResult.status == AudioDecoder::ReadStatus::EndOfStream) {
            break;
        }
    }

    if(!failed && dspChain && flushProcessingAtEnd) {
        ProcessingBufferList tailChunks;
        dspChain->flush(tailChunks, DspNode::FlushMode::EndOfTrack);
        encoderResult = writeProcessingChunks(encoder, tailChunks);
        if(!encoderResult.ok) {
            failed = true;
            error  = encoderResult.error;
        }
    }

    return {
        .ok                 = !failed,
        .cancelled          = false,
        .encoderInputFormat = encoderInputFormat,
        .error              = error,
        .warnings           = loaded.decoder->takeWarnings(),
    };
}

struct OutputPathDecision
{
    bool ready{false};
    ConversionResultStatus status{ConversionResultStatus::Failed};
    QString error;
};

OutputPathDecision prepareOutputPath(const ConversionRunner::Request& request, const ConversionPathResult& pathResult)
{
    if(pathResult.status == ConversionPathStatus::Skipped) {
        return {.ready = false, .status = ConversionResultStatus::Skipped, .error = {}};
    }

    bool ready = pathResult.status == ConversionPathStatus::Ready;

    if(pathResult.status == ConversionPathStatus::NeedsOverwriteDecision) {
        const ExistingFileMode decision = request.existingFileCallback
                                            ? request.existingFileCallback(pathResult.outputPath)
                                            : ExistingFileMode::Ask;
        if(decision == ExistingFileMode::Skip) {
            return {.ready = false, .status = ConversionResultStatus::Skipped, .error = {}};
        }

        if(decision != ExistingFileMode::Overwrite) {
            return {
                .status = ConversionResultStatus::Cancelled,
                .error  = u"Existing-file decision cancelled"_s,
            };
        }

        ready = true;
    }

    if(!ready) {
        return {
            .error = !pathResult.error.isEmpty() ? pathResult.error : u"Output path is not writable"_s,
        };
    }

    if(!QFileInfo{pathResult.outputPath}.absoluteDir().mkpath(u"."_s)) {
        return {.error = u"Failed to create output folder"_s};
    }

    return {.ready = true, .status = ConversionResultStatus::Failed, .error = {}};
}

QString transferPictures(const ConversionRunner::Request& request, bool supportsPictures, const Track& source,
                         const QString& outputPath)
{
    if(!request.job.preset.processing.transferPictures || !supportsPictures) {
        return {};
    }

    const TrackCovers covers = readCovers(*request.audioLoader, source);
    if(!covers.empty() && !request.audioLoader->writeTrackCover(Track{outputPath}, covers, AudioReader::None)) {
        return u"Failed to transfer attached pictures"_s;
    }

    return {};
}

QString transferMetadata(const ConversionRunner::Request& request, const Track& source, const QString& outputPath,
                         bool transferStatistics)
{
    AudioReader::WriteOptions options{AudioReader::None};
    if(request.job.preset.processing.transferMetadata) {
        options |= AudioReader::Metadata;
    }
    if(transferStatistics && request.job.preset.processing.transferRating) {
        options |= AudioReader::Rating;
    }
    if(transferStatistics && request.job.preset.processing.transferPlaycount) {
        options |= AudioReader::Playcount;
    }
    if(options == AudioReader::None) {
        return {};
    }

    Track outputTrack{source};
    outputTrack.setFilePath(outputPath);
    if(!request.audioLoader->writeTrackMetadata(outputTrack, options)) {
        return u"Failed to transfer metadata"_s;
    }

    return {};
}

void appendGroupedResults(std::vector<ConversionTrackResult>& results, const TrackList& tracks,
                          const QString& outputPath, ConversionResultStatus status, const QString& error,
                          const QStringList& firstTrackWarnings = {})
{
    for(size_t i{0}; i < tracks.size(); ++i) {
        results.push_back({
            .sourceTrack = tracks[i],
            .outputPath  = outputPath,
            .status      = status,
            .error       = error,
            .warnings    = i == 0 ? firstTrackWarnings : QStringList{},
        });
    }
}

std::vector<ConversionTrackResult> runGroupedOutput(const ConversionRunner::Request& request,
                                                    const ConversionPathResult& pathResult, int groupStartIndex,
                                                    bool supportsPictures)
{
    const TrackList& groupTracks = pathResult.tracks;
    const auto makeResults       = [&](ConversionResultStatus status, const QString& error,
                                       const QStringList& firstTrackWarnings = QStringList{}) {
        std::vector<ConversionTrackResult> results;
        results.reserve(groupTracks.size());
        appendGroupedResults(results, groupTracks, pathResult.outputPath, status, error, firstTrackWarnings);
        return results;
    };

    if(groupTracks.empty()) {
        return {failedResult({}, pathResult.outputPath, u"Output group contains no tracks"_s)};
    }

    if(shouldCancel(request)) {
        return makeResults(ConversionResultStatus::Cancelled, {});
    }

    const OutputPathDecision pathDecision = prepareOutputPath(request, pathResult);
    if(!pathDecision.ready) {
        return makeResults(pathDecision.status, pathDecision.error);
    }

    OutputTransaction output{pathResult.outputPath};
    QString outputError;
    if(!output.prepare(outputError)) {
        return makeResults(ConversionResultStatus::Failed, outputError);
    }

    auto encoder = request.encoderRegistry->createEncoder(request.job.preset.encoder.profile.id);
    if(!encoder) {
        return makeResults(ConversionResultStatus::Failed, u"No encoder available"_s);
    }

    const bool preserveProcessingState = request.job.preset.processing.preserveDspStateBetweenTracks;
    SharedProcessingContext sharedProcessing;
    AudioFormat combinedFormat;
    TrackEncodingResult encoding;
    QStringList sourceWarnings;

    for(int i{0}; std::cmp_less(i, groupTracks.size()); ++i) {
        const Track& track           = groupTracks[i];
        const bool finalTrackInGroup = std::cmp_equal(i + 1, groupTracks.size());

        encoding = encodeTrack(request, track, groupStartIndex + i, pathResult.outputPath, output.path(), *encoder,
                               i == 0, combinedFormat, preserveProcessingState ? &sharedProcessing : nullptr,
                               !preserveProcessingState || finalTrackInGroup);
        sourceWarnings.append(encoding.warnings);
        if(!encoding.ok) {
            break;
        }

        if(i == 0) {
            combinedFormat = encoding.encoderInputFormat;
        }
    }

    AudioEncoder::Result finishResult = AudioEncoder::Result::success();
    if(encoding.ok) {
        finishResult = encoder->finish();
    }

    if(!encoding.ok || !finishResult.ok) {
        const QString error = !encoding.ok ? encoding.error : finishResult.error;
        encoder.reset();
        return makeResults(encoding.cancelled ? ConversionResultStatus::Cancelled : ConversionResultStatus::Failed,
                           error, sourceWarnings);
    }

    if(shouldCancel(request)) {
        return makeResults(ConversionResultStatus::Cancelled, u"Conversion cancelled"_s, sourceWarnings);
    }

    QStringList warnings{sourceWarnings};
    const QString metadataError = transferMetadata(request, groupTracks.front(), output.path(), false);
    if(!metadataError.isEmpty()) {
        warnings.append(metadataError);
    }
    const QString pictureError = transferPictures(request, supportsPictures, groupTracks.front(), output.path());
    if(!pictureError.isEmpty()) {
        warnings.append(pictureError);
    }

    if(request.job.preset.other.verifyOutput) {
        const QString verificationError = verifyOutput(request, output.path());
        if(!verificationError.isEmpty()) {
            return makeResults(shouldCancel(request) ? ConversionResultStatus::Cancelled
                                                     : ConversionResultStatus::Failed,
                               verificationError, warnings);
        }
    }

    if(shouldCancel(request)) {
        return makeResults(ConversionResultStatus::Cancelled, u"Conversion cancelled"_s, warnings);
    }
    if(!output.commit(outputError)) {
        return makeResults(ConversionResultStatus::Failed, outputError, warnings);
    }

    return makeResults(ConversionResultStatus::Succeeded, {}, warnings);
}

ConversionTrackResult runIndividualOutput(const ConversionRunner::Request& request,
                                          const ConversionPathResult& pathResult, size_t i, bool supportsPictures)
{
    const Track& track = request.job.tracks[i];

    if(shouldCancel(request)) {
        return {
            .sourceTrack = track,
            .outputPath  = pathResult.outputPath,
            .status      = ConversionResultStatus::Cancelled,
            .error       = {},
            .warnings    = {},
        };
    }

    const OutputPathDecision pathDecision = prepareOutputPath(request, pathResult);
    if(!pathDecision.ready) {
        return {
            .sourceTrack = track,
            .outputPath  = pathResult.outputPath,
            .status      = pathDecision.status,
            .error       = pathDecision.error,
            .warnings    = {},
        };
    }

    OutputTransaction output{pathResult.outputPath};
    QString outputError;
    if(!output.prepare(outputError)) {
        return failedResult(track, pathResult.outputPath, outputError);
    }

    auto encoder = request.encoderRegistry->createEncoder(request.job.preset.encoder.profile.id);
    if(!encoder) {
        return failedResult(track, pathResult.outputPath, u"No encoder available"_s);
    }

    const TrackEncodingResult encoding
        = encodeTrack(request, track, static_cast<int>(i), pathResult.outputPath, output.path(), *encoder, true, {});

    AudioEncoder::Result finishResult = AudioEncoder::Result::success();
    if(encoding.ok) {
        finishResult = encoder->finish();
    }

    if(!encoding.ok || !finishResult.ok) {
        const QString error = !encoding.ok ? encoding.error : finishResult.error;
        encoder.reset();
        return {
            .sourceTrack = track,
            .outputPath  = pathResult.outputPath,
            .status      = encoding.cancelled ? ConversionResultStatus::Cancelled : ConversionResultStatus::Failed,
            .error       = error,
            .warnings    = encoding.warnings,
        };
    }

    if(shouldCancel(request)) {
        return {
            .sourceTrack = track,
            .outputPath  = pathResult.outputPath,
            .status      = ConversionResultStatus::Cancelled,
            .error       = u"Conversion cancelled"_s,
            .warnings    = encoding.warnings,
        };
    }

    QStringList warnings{encoding.warnings};
    const QString metadataError = transferMetadata(request, track, output.path(), true);
    if(!metadataError.isEmpty()) {
        warnings.append(metadataError);
    }
    const QString pictureError = transferPictures(request, supportsPictures, track, output.path());
    if(!pictureError.isEmpty()) {
        warnings.append(pictureError);
    }

    if(request.job.preset.other.verifyOutput) {
        QString verificationError = verifyOutput(request, output.path());
        if(verificationError.isEmpty() && supportsLosslessPcmVerification(request.job.preset)) {
            verificationError = verifyLosslessPcm(request, track, output.path(), request.job.preset);
        }
        if(!verificationError.isEmpty()) {
            return {.sourceTrack = track,
                    .outputPath  = pathResult.outputPath,
                    .status
                    = shouldCancel(request) ? ConversionResultStatus::Cancelled : ConversionResultStatus::Failed,
                    .error    = verificationError,
                    .warnings = warnings};
        }
    }

    if(shouldCancel(request)) {
        return {.sourceTrack = track,
                .outputPath  = pathResult.outputPath,
                .status      = ConversionResultStatus::Cancelled,
                .error       = u"Conversion cancelled"_s,
                .warnings    = warnings};
    }

    if(!output.commit(outputError)) {
        return {.sourceTrack = track,
                .outputPath  = pathResult.outputPath,
                .status      = ConversionResultStatus::Failed,
                .error       = outputError,
                .warnings    = warnings};
    }

    return {
        .sourceTrack = track,
        .outputPath  = pathResult.outputPath,
        .status      = ConversionResultStatus::Succeeded,
        .error       = {},
        .warnings    = std::move(warnings),
    };
}

QString pathIdentity(const QString& path)
{
    const QFileInfo info{path};
    const QString canonical = info.canonicalFilePath();
    if(!canonical.isEmpty()) {
        return canonical.toCaseFolded();
    }

    const QString dir = info.absoluteDir().canonicalPath();
    return QDir::cleanPath(dir.isEmpty() ? info.absoluteFilePath() : QDir{dir}.filePath(info.fileName()))
        .toCaseFolded();
}

int parallelConversionCount(const ConversionRunner::Request& request,
                            const std::vector<ConversionPathResult>& pathResults, int requested)
{
    if(requested == 1 || request.sourceObserver || pathResults.size() < 2) {
        return 1;
    }

    std::set<QString> sources;
    for(const Track& track : request.job.tracks) {
        // Device-backed sources and archive wrappers retain their sequential behaviour
        if(track.isRemote() || track.isVirtual() || track.isInArchive()) {
            return 1;
        }
        sources.insert(pathIdentity(track.filepath()));
    }

    std::set<QString> destinations;
    for(const auto& path : pathResults) {
        const QString destination = pathIdentity(path.outputPath);
        if(sources.contains(destination) || !destinations.insert(destination).second) {
            return 1;
        }
    }

    const int limit = requested <= 0 ? static_cast<int>(std::max(1U, std::thread::hardware_concurrency())) : requested;
    return std::min(limit, static_cast<int>(pathResults.size()));
}

std::vector<ConversionTrackResult> runOutputs(const ConversionRunner::Request& request,
                                              const std::vector<ConversionPathResult>& pathResults,
                                              bool supportsPictures)
{
    const bool individualOutputs = request.job.preset.destination.outputStyle == OutputStyle::IndividualFiles;
    std::vector<std::vector<ConversionTrackResult>> outputResults(pathResults.size());

    std::vector trackOffsets{0};
    trackOffsets.reserve(pathResults.size() + 1);
    for(const auto& path : pathResults) {
        trackOffsets.push_back(trackOffsets.back() + static_cast<int>(path.tracks.size()));
    }

    std::mutex callbackMutex;
    std::mutex overwriteMutex;
    std::atomic_bool cancelled{false};
    std::atomic_size_t nextIndex{0};

    ConversionRunner::Request workerRequest{request};
    workerRequest.cancelCallback = [&] {
        const std::scoped_lock lock{callbackMutex};
        if(request.cancelCallback && request.cancelCallback()) {
            cancelled.store(true, std::memory_order_relaxed);
        }
        return cancelled.load(std::memory_order_relaxed);
    };
    workerRequest.progressCallback = [&](const ConversionProgress& progress) {
        const std::scoped_lock lock{callbackMutex};
        if(request.progressCallback) {
            request.progressCallback(progress);
        }
    };
    workerRequest.existingFileCallback = [&](const QString& path) {
        const std::scoped_lock lock{overwriteMutex};
        if(shouldCancel(workerRequest)) {
            return ExistingFileMode::Ask;
        }
        const auto decision = request.existingFileCallback ? request.existingFileCallback(path) : ExistingFileMode::Ask;
        if(decision == ExistingFileMode::Ask) {
            cancelled.store(true, std::memory_order_relaxed);
        }
        return decision;
    };

    const auto work = [&] {
        while(true) {
            const size_t i = nextIndex.fetch_add(1, std::memory_order_relaxed);
            if(i >= outputResults.size()) {
                return;
            }

            if(individualOutputs) {
                outputResults[i].push_back(runIndividualOutput(workerRequest, pathResults[i], i, supportsPictures));
            }
            else {
                outputResults[i] = runGroupedOutput(workerRequest, pathResults[i], trackOffsets[i], supportsPictures);
            }

            for(int j{0}; std::cmp_less(j, outputResults[i].size()); ++j) {
                const auto& result = outputResults[i][j];
                reportProgress(workerRequest, trackOffsets[i] + j, result.sourceTrack, result.outputPath,
                               result.sourceTrack.duration(), true);
            }
        }
    };

    const int count = parallelConversionCount(request, pathResults, request.maxParallelConversions);
    if(count == 1) {
        work();
    }
    else {
        std::vector<std::future<void>> workers;
        workers.reserve(count);
        for(int i{0}; i < count; ++i) {
            workers.push_back(std::async(std::launch::async, work));
        }
        for(auto& worker : workers) {
            worker.get();
        }
    }

    std::vector<ConversionTrackResult> results;
    results.reserve(request.job.tracks.size());

    std::set<QString> handledSidecarCopies;
    for(size_t i{0}; i < outputResults.size(); ++i) {
        auto& groupResults = outputResults[i];
        if(!groupResults.empty() && groupResults.front().status == ConversionResultStatus::Succeeded
           && !shouldCancel(workerRequest)) {
            const QStringList warnings
                = copySidecarFiles(request.job.preset.other.copyFilesPattern, pathResults[i].tracks,
                                   pathResults[i].outputPath, handledSidecarCopies);
            groupResults.front().warnings.append(warnings);
        }
        for(auto& result : groupResults) {
            results.push_back(std::move(result));
        }
    }
    return results;
}

} // namespace

namespace ConversionRunner {
std::vector<ConversionTrackResult> run(const Request& request)
{
    std::vector<ConversionTrackResult> results;
    results.reserve(request.job.tracks.size());

    if(!request.audioLoader) {
        results.push_back(failedResult({}, {}, u"No audio loader supplied"_s));
        return results;
    }

    if(!request.encoderRegistry) {
        results.push_back(failedResult({}, {}, u"No encoder registry supplied"_s));
        return results;
    }

    ConversionPathResolver::Request pathRequest;
    pathRequest.tracks      = request.job.tracks;
    pathRequest.destination = request.job.preset.destination;
    pathRequest.extension   = request.job.preset.encoder.profile.extension;
    pathRequest.askFolder   = request.askFolder;

    const auto pathResults       = ConversionPathResolver::resolve(pathRequest);
    const bool individualOutputs = request.job.preset.destination.outputStyle == OutputStyle::IndividualFiles;
    if((individualOutputs && pathResults.size() != request.job.tracks.size())
       || (!request.job.tracks.empty() && pathResults.empty())) {
        results.push_back(failedResult({}, {}, u"Failed to resolve output paths"_s));
        return results;
    }

    const bool supportsPictures
        = profileSupportsPictures(*request.encoderRegistry, request.job.preset.encoder.profile.id);
    return runOutputs(request, pathResults, supportsPictures);
}
} // namespace ConversionRunner
} // namespace Fooyin
