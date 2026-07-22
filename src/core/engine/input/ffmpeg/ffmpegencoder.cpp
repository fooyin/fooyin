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

#include "ffmpegencoder.h"

#include "ffmpegutils.h"

#include <QStringList>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/channel_layout.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

#include <array>
#include <span>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
struct FFmpegProfileDescriptor
{
    QString id;
    QString name;
    QString extension;
    QString containerName;
    QStringList codecNames;
    EncoderMode mode{EncoderMode::Default};
    int bitrateKbps{0};
    double quality{0.0};
    int compressionLevel{-1};
    EncoderCapabilities capabilities;
    QString settingsSummary;
    std::function<int(const EncoderProfile&)> estimateBitrateKbps;
    bool supportsPictures{false};
};

int estimateMp3BitrateKbps(const EncoderProfile& profile)
{
    static constexpr std::array Estimates{245, 225, 190, 175, 165, 130, 115, 100, 85, 65};

    const double quality = std::clamp(profile.quality, 0.0, 9.0);
    const auto lower     = static_cast<size_t>(std::floor(quality));
    const auto upper     = static_cast<size_t>(std::ceil(quality));
    if(lower == upper) {
        return Estimates.at(lower);
    }

    const double fraction = quality - static_cast<double>(lower);
    return static_cast<int>(std::lround(std::lerp(Estimates.at(lower), Estimates.at(upper), fraction)));
}

int estimateVorbisBitrateKbps(const EncoderProfile& profile)
{
    static constexpr std::array Estimates{64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 400};

    const double quality = std::clamp(profile.quality, 0.0, 10.0);
    const auto lower     = static_cast<size_t>(std::floor(quality));
    const auto upper     = static_cast<size_t>(std::ceil(quality));
    if(lower == upper) {
        return Estimates.at(lower);
    }

    const double fraction = quality - static_cast<double>(lower);
    return static_cast<int>(std::lround(std::lerp(Estimates.at(lower), Estimates.at(upper), fraction)));
}

const AVCodec* findEncoder(const QStringList& codecNames, QString* selectedName = nullptr)
{
    for(const QString& codecName : codecNames) {
        if(const AVCodec* codec = avcodec_find_encoder_by_name(codecName.toUtf8().constData())) {
            if(selectedName) {
                *selectedName = codecName;
            }
            return codec;
        }
    }

    return nullptr;
}

bool encoderSupportsOption(const AVCodecContext* context, const char* name)
{
    return context && context->priv_data
        && av_opt_find(context->priv_data, name, nullptr, AV_OPT_FLAG_ENCODING_PARAM, 0) != nullptr;
}

bool encoderSupportsOption(const AVCodec* codec, const char* name)
{
    AVCodecContext* context = avcodec_alloc_context3(codec);
    const bool supported    = encoderSupportsOption(context, name);
    avcodec_free_context(&context);
    return supported;
}

QStringList codecNamesForSettings(const AudioEncoderSettings& settings)
{
    if(settings.profile.id != u"ffmpeg-wav"_s) {
        return {settings.profile.codecName};
    }

    switch(settings.outputSampleFormat) {
        case SampleFormat::U8:
            return {u"pcm_u8"_s};
        case SampleFormat::S24In32:
            return {u"pcm_s24le"_s};
        case SampleFormat::S32:
            return {u"pcm_s32le"_s};
        case SampleFormat::F32:
            return {u"pcm_f32le"_s};
        case SampleFormat::F64:
            return {u"pcm_f64le"_s};
        case SampleFormat::S16:
        case SampleFormat::Unknown:
        default:
            return {u"pcm_s16le"_s};
    }
}

bool hasMuxer(const QString& containerName, const QString& extension)
{
    return av_guess_format(containerName.toUtf8().constData(), nullptr, extension.toUtf8().constData()) != nullptr;
}

AudioEncoder::Result errorResult(const QString& prefix, int error)
{
    return AudioEncoder::Result::failure(prefix + u": "_s + Utils::ffmpegErrorString(error));
}

std::span<const AVSampleFormat> supportedSampleFormats(const AVCodec* codec)
{
    if(!codec) {
        return {};
    }

#if !OLD_CODEC_SUPPORTED_CONFIG
    const void* configs{nullptr};
    int count{0};
    if(avcodec_get_supported_config(nullptr, codec, AV_CODEC_CONFIG_SAMPLE_FORMAT, 0, &configs, &count) < 0 || !configs
       || count <= 0) {
        return {};
    }
    return {static_cast<const AVSampleFormat*>(configs), static_cast<size_t>(count)};
#else
    const AVSampleFormat* formats{codec->sample_fmts};
    if(!formats) {
        return {};
    }
    const AVSampleFormat* end{formats};
    while(*end != AV_SAMPLE_FMT_NONE) {
        ++end;
    }
    return {formats, static_cast<size_t>(end - formats)};
#endif
}

std::span<const int> supportedSampleRates(const AVCodec* codec)
{
    if(!codec) {
        return {};
    }

#if !OLD_CODEC_SUPPORTED_CONFIG
    const void* configs{nullptr};
    int count{0};
    if(avcodec_get_supported_config(nullptr, codec, AV_CODEC_CONFIG_SAMPLE_RATE, 0, &configs, &count) < 0 || !configs
       || count <= 0) {
        return {};
    }
    return {static_cast<const int*>(configs), static_cast<size_t>(count)};
#else
    const int* rates = codec->supported_samplerates;
    if(!rates) {
        return {};
    }
    const int* end{rates};
    while(*end != 0) {
        ++end;
    }
    return {rates, static_cast<size_t>(end - rates)};
#endif
}

AVSampleFormat selectSampleFormat(const AVCodec* codec, SampleFormat requested)
{
    const auto formats = supportedSampleFormats(codec);
    if(formats.empty()) {
        return requested == SampleFormat::Unknown ? AV_SAMPLE_FMT_S16 : Utils::sampleFormat(requested);
    }

    if(requested != SampleFormat::Unknown) {
        for(const AVSampleFormat format : formats) {
            const int bitsPerSample = requested == SampleFormat::S24In32 ? 24 : 0;
            if(Utils::sampleFormat(format, bitsPerSample) == requested) {
                return format;
            }
        }
        return AV_SAMPLE_FMT_NONE;
    }

    constexpr std::array preferredFormats{
        SampleFormat::S16, SampleFormat::S32, SampleFormat::F32, SampleFormat::U8, SampleFormat::F64,
    };

    for(const SampleFormat preferred : preferredFormats) {
        for(const AVSampleFormat format : formats) {
            if(Utils::sampleFormat(format, 0) == preferred) {
                return format;
            }
        }
    }

    return formats.front();
}

int selectSampleRate(const AVCodec* codec, int requestedRate)
{
    const int safeRate = std::max(1, requestedRate);
    const auto rates   = supportedSampleRates(codec);
    if(rates.empty()) {
        return safeRate;
    }

    for(const int rate : rates) {
        if(rate == safeRate) {
            return safeRate;
        }
    }

    return rates.front() > 0 ? rates.front() : safeRate;
}

#if OLD_CHANNEL_LAYOUT
uint64_t defaultChannelLayout(int channels)
{
    const auto layout = av_get_default_channel_layout(std::max(1, channels));
    return layout > 0 ? static_cast<uint64_t>(layout) : 0;
}
#else
AVChannelLayout defaultChannelLayout(int channels)
{
    AVChannelLayout layout{};
    av_channel_layout_default(&layout, std::max(1, channels));
    return layout;
}
#endif

EncoderProfile makeProfile(const FFmpegProfileDescriptor& descriptor, const QString& codecName)
{
    EncoderProfile profile;
    profile.id               = descriptor.id;
    profile.name             = descriptor.name;
    profile.extension        = descriptor.extension;
    profile.containerName    = descriptor.containerName;
    profile.codecName        = codecName;
    profile.mode             = descriptor.mode;
    profile.bitrateKbps      = descriptor.bitrateKbps;
    profile.quality          = descriptor.quality;
    profile.compressionLevel = descriptor.compressionLevel;
    return profile;
}

QString description(const FFmpegProfileDescriptor& descriptor)
{
    if(!descriptor.settingsSummary.isEmpty()) {
        return descriptor.settingsSummary;
    }
    if(descriptor.bitrateKbps > 0) {
        return QString::number(descriptor.bitrateKbps) + u" kbps"_s;
    }
    if(descriptor.compressionLevel >= 0) {
        return u"level %1"_s.arg(descriptor.compressionLevel);
    }
    return {};
}

std::vector<FFmpegProfileDescriptor> builtInProfiles()
{
    return {
        {
            .id               = u"ffmpeg-wav"_s,
            .name             = u"WAV"_s,
            .extension        = u"wav"_s,
            .containerName    = u"wav"_s,
            .codecNames       = {u"pcm_s16le"_s},
            .mode             = EncoderMode::Default,
            .capabilities     = {
                    .modes            = {EncoderMode::Default},
                    .bitrateKbps      = {},
                    .quality          = {},
                    .compressionLevel = {},
            },
            .settingsSummary  = u"PCM 16-bit"_s,
            .estimateBitrateKbps = {},
            .supportsPictures = false,
        },
        {
            .id               = u"ffmpeg-flac"_s,
            .name             = u"FLAC"_s,
            .extension        = u"flac"_s,
            .containerName    = u"flac"_s,
            .codecNames       = {u"flac"_s},
            .mode             = EncoderMode::LosslessCompression,
            .compressionLevel = 5,
            .capabilities     = {
                    .modes            = {EncoderMode::LosslessCompression},
                    .bitrateKbps      = {},
                    .quality          = {},
                    .compressionLevel = {.minimum = 0, .maximum = 12, .step = 1},
            },
            .settingsSummary  = u"level 5"_s,
            .estimateBitrateKbps = {},
            .supportsPictures = true,
        },
        {
            .id               = u"ffmpeg-alac"_s,
            .name             = u"Apple Lossless"_s,
            .extension        = u"m4a"_s,
            .containerName    = u"ipod"_s,
            .codecNames       = {u"alac"_s},
            .mode             = EncoderMode::LosslessCompression,
            .capabilities     = {
                    .modes            = {EncoderMode::LosslessCompression},
                    .bitrateKbps      = {},
                    .quality          = {},
                    .compressionLevel = {},
            },
            .settingsSummary  = u"Lossless"_s,
            .estimateBitrateKbps = {},
            .supportsPictures = true,
        },
        {
            .id               = u"ffmpeg-wavpack"_s,
            .name             = u"WavPack"_s,
            .extension        = u"wv"_s,
            .containerName    = u"wv"_s,
            .codecNames       = {u"wavpack"_s},
            .mode             = EncoderMode::LosslessCompression,
            .compressionLevel = 2,
            .capabilities     = {
                    .modes            = {EncoderMode::LosslessCompression},
                    .bitrateKbps      = {},
                    .quality          = {},
                    .compressionLevel = {.minimum = 0, .maximum = 8, .step = 1},
            },
            .settingsSummary  = u"level 2"_s,
            .estimateBitrateKbps = {},
            .supportsPictures = true,
        },
        {
            .id               = u"ffmpeg-mp3"_s,
            .name             = u"MP3"_s,
            .extension        = u"mp3"_s,
            .containerName    = u"mp3"_s,
            .codecNames       = {u"libmp3lame"_s, u"mp3"_s},
            .mode             = EncoderMode::ConstantBitrate,
            .bitrateKbps      = 192,
            .quality          = 2.0,
            .compressionLevel = 2,
            .capabilities     = {
                    .modes            = {EncoderMode::ConstantQuality, EncoderMode::AverageBitrate,
                                         EncoderMode::ConstantBitrate},
                    .bitrateKbps      = {.minimum = 32, .maximum = 320, .step = 16},
                    .quality          = {.minimum = 0, .maximum = 9, .step = 1},
                    .compressionLevel = {.minimum = 0, .maximum = 9, .step = 1},
            },
            .settingsSummary  = u"CBR 192 kbps"_s,
            .estimateBitrateKbps = estimateMp3BitrateKbps,
            .supportsPictures = true,
        },
        {
            .id             = u"ffmpeg-aac"_s,
            .name           = u"AAC"_s,
            .extension      = u"m4a"_s,
            .containerName  = u"ipod"_s,
            .codecNames     = {u"aac"_s},
            .mode           = EncoderMode::ConstantBitrate,
            .bitrateKbps    = 256,
            .quality        = 2.0,
            .capabilities   = {
                  .modes            = {EncoderMode::ConstantQuality, EncoderMode::ConstantBitrate},
                  .bitrateKbps      = {.minimum = 32, .maximum = 512, .step = 16},
                  .quality          = {.minimum = 0.1, .maximum = 5.0, .step = 0.1},
                  .compressionLevel = {},
            },
            .settingsSummary  = u"CBR 256 kbps"_s,
            .estimateBitrateKbps = {},
            .supportsPictures = true,
        },
        {
            .id             = u"ffmpeg-vorbis"_s,
            .name           = u"Ogg Vorbis"_s,
            .extension      = u"ogg"_s,
            .containerName  = u"ogg"_s,
            .codecNames     = {u"libvorbis"_s},
            .mode           = EncoderMode::ConstantQuality,
            .quality        = 5.0,
            .capabilities   = {
                  .modes            = {EncoderMode::ConstantQuality},
                  .bitrateKbps      = {},
                  .quality          = {.minimum = 0, .maximum = 10, .step = 1},
                  .compressionLevel = {},
            },
            .settingsSummary      = u"quality 5"_s,
            .estimateBitrateKbps = estimateVorbisBitrateKbps,
            .supportsPictures     = true,
        },
        {
            .id               = u"ffmpeg-opus"_s,
            .name             = u"Opus"_s,
            .extension        = u"opus"_s,
            .containerName    = u"ogg"_s,
            .codecNames       = {u"libopus"_s, u"opus"_s},
            .mode             = EncoderMode::VariableBitrate,
            .bitrateKbps      = 128,
            .compressionLevel = 10,
            .capabilities     = {
                    .modes            = {EncoderMode::VariableBitrate, EncoderMode::ConstrainedVariableBitrate,
                                         EncoderMode::ConstantBitrate},
                    .bitrateKbps      = {.minimum = 6, .maximum = 510, .step = 8},
                    .quality          = {},
                    .compressionLevel = {.minimum = 0, .maximum = 10, .step = 1},
            },
            .settingsSummary  = u"VBR 128 kbps"_s,
            .estimateBitrateKbps = {},
            .supportsPictures = true,
        },
    };
}

} // namespace

FFmpegEncoder::FFmpegEncoder()
    : m_stream{nullptr}
    , m_inputSampleFormat{AV_SAMPLE_FMT_NONE}
    , m_outputSampleFormat{AV_SAMPLE_FMT_NONE}
    , m_outputSampleRate{0}
    , m_nextPts{0}
    , m_dither{false}
    , m_finished{false}
{ }

AudioEncoder::Result FFmpegEncoder::init(const QString& outputPath, const AudioFormat& input,
                                         const AudioEncoderSettings& settings)
{
    cleanup();

    if(outputPath.isEmpty()) {
        return Result::failure(u"Output path is empty"_s);
    }
    if(!input.isValid()) {
        return Result::failure(u"Input format is invalid"_s);
    }

    QString codecName;
    const AVCodec* codec = findEncoder(codecNamesForSettings(settings), &codecName);
    if(!codec) {
        return Result::failure(u"Encoder is not available"_s);
    }

    const QByteArray containerName = settings.profile.containerName.toUtf8();
    const QByteArray pathBytes     = outputPath.toLocal8Bit();

    AVFormatContext* formatContext{nullptr};
    int ret = avformat_alloc_output_context2(&formatContext, nullptr, containerName.constData(), pathBytes.constData());
    m_formatContext.reset(formatContext);
    if(ret < 0 || !m_formatContext) {
        return errorResult(u"Failed to allocate output context"_s, ret);
    }

    m_stream = avformat_new_stream(m_formatContext.get(), nullptr);
    if(!m_stream) {
        return Result::failure(u"Failed to create output stream"_s);
    }

    m_codecContext.reset(avcodec_alloc_context3(codec));
    if(!m_codecContext) {
        return Result::failure(u"Failed to allocate encoder context"_s);
    }

    m_outputSampleFormat = selectSampleFormat(codec, settings.outputSampleFormat);
    if(m_outputSampleFormat == AV_SAMPLE_FMT_NONE) {
        return Result::failure(u"Requested output sample format is unsupported by the encoder"_s);
    }

    m_outputSampleRate          = selectSampleRate(codec, input.sampleRate());
    m_codecContext->codec_type  = AVMEDIA_TYPE_AUDIO;
    m_codecContext->codec_id    = codec->id;
    m_codecContext->sample_fmt  = m_outputSampleFormat;
    m_codecContext->sample_rate = m_outputSampleRate;
    m_codecContext->time_base   = AVRational{.num = 1, .den = m_outputSampleRate};
    m_stream->time_base         = m_codecContext->time_base;

    if(settings.outputSampleFormat == SampleFormat::S24In32) {
        m_codecContext->bits_per_raw_sample = 24;
    }

    m_dither = settings.ditherMode == DitherMode::Always;

    const bool constantQuality = settings.profile.mode == EncoderMode::ConstantQuality;
    if(constantQuality) {
        m_codecContext->flags |= AV_CODEC_FLAG_QSCALE;
        m_codecContext->global_quality = static_cast<int>(settings.profile.quality * FF_QP2LAMBDA);
    }
    else if(settings.profile.bitrateKbps > 0) {
        m_codecContext->bit_rate = static_cast<int64_t>(settings.profile.bitrateKbps) * 1000;
    }

    const int safeChannelCount = std::max(1, input.channelCount());

#if OLD_CHANNEL_LAYOUT
    m_codecContext->channels       = safeChannelCount;
    m_codecContext->channel_layout = defaultChannelLayout(safeChannelCount);
#else
    m_codecContext->ch_layout = defaultChannelLayout(safeChannelCount);
#endif

    if((m_formatContext->oformat->flags & AVFMT_GLOBALHEADER) != 0) {
        m_codecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    AVDictionary* options{nullptr};

    const bool supportsVbr = encoderSupportsOption(m_codecContext.get(), "vbr");
    switch(settings.profile.mode) {
        case EncoderMode::VariableBitrate:
            if(supportsVbr) {
                av_dict_set(&options, "vbr", "on", 0);
            }
            break;
        case EncoderMode::ConstrainedVariableBitrate:
            if(!supportsVbr) {
                return Result::failure(u"The selected encoder does not support constrained VBR mode"_s);
            }
            av_dict_set(&options, "vbr", "constrained", 0);
            break;
        case EncoderMode::ConstantBitrate:
            if(supportsVbr) {
                av_dict_set(&options, "vbr", "off", 0);
            }
            break;
        case EncoderMode::AverageBitrate:
            if(!encoderSupportsOption(m_codecContext.get(), "abr")) {
                return Result::failure(u"The selected encoder does not support ABR mode"_s);
            }
            av_dict_set(&options, "abr", "1", 0);
            break;
        case EncoderMode::Default:
        case EncoderMode::ConstantQuality:
        case EncoderMode::LosslessCompression:
            break;
    }

    if(settings.profile.compressionLevel >= 0) {
        av_dict_set(&options, "compression_level", QByteArray::number(settings.profile.compressionLevel).constData(),
                    0);
    }

    ret = avcodec_open2(m_codecContext.get(), codec, &options);
    av_dict_free(&options);
    if(ret < 0) {
        return errorResult(u"Failed to open encoder"_s, ret);
    }

    ret = avcodec_parameters_from_context(m_stream->codecpar, m_codecContext.get());
    if(ret < 0) {
        return errorResult(u"Failed to copy encoder parameters"_s, ret);
    }

    m_frame.reset(av_frame_alloc());
    m_convertedFrame.reset(av_frame_alloc());
    m_packet.reset(av_packet_alloc());

    if(!m_frame || !m_convertedFrame || !m_packet) {
        return Result::failure(u"Failed to allocate encoder frame or packet"_s);
    }

    if(m_codecContext->frame_size > 0) {
#if OLD_CHANNEL_LAYOUT
        const int channels = m_codecContext->channels;
#else
        const int channels = m_codecContext->ch_layout.nb_channels;
#endif
        m_fifo.reset(av_audio_fifo_alloc(m_outputSampleFormat, channels, m_codecContext->frame_size));
        if(!m_fifo) {
            return Result::failure(u"Failed to allocate encoder audio FIFO"_s);
        }
    }

    m_inputFormat       = input;
    m_inputSampleFormat = Utils::sampleFormat(input.sampleFormat());
    if(m_inputSampleFormat == AV_SAMPLE_FMT_NONE) {
        return Result::failure(u"Unsupported input sample format"_s);
    }

    if(const Result result = configureResampler(); !result.ok) {
        return result;
    }

    if((m_formatContext->oformat->flags & AVFMT_NOFILE) == 0) {
        ret = avio_open(&m_formatContext->pb, pathBytes.constData(), AVIO_FLAG_WRITE);
        if(ret < 0) {
            return errorResult(u"Failed to open output file"_s, ret);
        }
    }

    ret = avformat_write_header(m_formatContext.get(), nullptr);
    if(ret < 0) {
        return errorResult(u"Failed to write output header"_s, ret);
    }

    m_nextPts  = 0;
    m_finished = false;
    return Result::success();
}

AudioEncoder::Result FFmpegEncoder::configureResampler()
{
#if OLD_CHANNEL_LAYOUT
    const uint64_t inLayout  = defaultChannelLayout(m_inputFormat.channelCount());
    const uint64_t outLayout = m_codecContext->channel_layout;
    m_swr.reset(swr_alloc_set_opts(nullptr, static_cast<int64_t>(outLayout), m_outputSampleFormat, m_outputSampleRate,
                                   static_cast<int64_t>(inLayout), m_inputSampleFormat, m_inputFormat.sampleRate(), 0,
                                   nullptr));
#else
    AVChannelLayout inLayout = defaultChannelLayout(m_inputFormat.channelCount());
    SwrContext* swr{nullptr};
    const int ret = swr_alloc_set_opts2(&swr, &m_codecContext->ch_layout, m_outputSampleFormat, m_outputSampleRate,
                                        &inLayout, m_inputSampleFormat, m_inputFormat.sampleRate(), 0, nullptr);
    av_channel_layout_uninit(&inLayout);
    if(ret < 0) {
        swr_free(&swr);
        return errorResult(u"Failed to allocate resampler"_s, ret);
    }
    m_swr.reset(swr);
#endif

    if(!m_swr) {
        return Result::failure(u"Failed to allocate resampler"_s);
    }

    if(m_dither && av_opt_set_int(m_swr.get(), "dither_method", SWR_DITHER_TRIANGULAR, 0) < 0) {
        return Result::failure(u"Failed to configure encoder dither"_s);
    }

    const int initRet = swr_init(m_swr.get());
    if(initRet < 0) {
        return errorResult(u"Failed to initialise resampler"_s, initRet);
    }

    return Result::success();
}

AudioEncoder::Result FFmpegEncoder::write(const AudioBuffer& buffer)
{
    if(m_finished || !m_codecContext || !m_swr || !m_frame) {
        return Result::failure(u"Encoder is not initialised"_s);
    }
    if(!buffer.isValid()) {
        return Result::failure(u"Audio buffer is invalid"_s);
    }
    if(buffer.format() != m_inputFormat) {
        return Result::failure(u"Audio buffer format changed during encoding"_s);
    }

    return encodeInput(reinterpret_cast<const uint8_t*>(buffer.data()), buffer.frameCount());
}

AudioEncoder::Result FFmpegEncoder::encodeInput(const uint8_t* inputData, int inputFrames)
{
    if(!inputData || inputFrames <= 0) {
        return Result::success();
    }

    const int maxOutFrames = m_inputFormat.sampleRate() == m_outputSampleRate
                               ? inputFrames
                               : std::max(1, swr_get_out_samples(m_swr.get(), inputFrames));

    if(const Result result = prepareFrame(m_convertedFrame.get(), maxOutFrames); !result.ok) {
        return result;
    }

    const uint8_t* inputPlanes[]{inputData};
    const int converted = swr_convert(m_swr.get(), m_convertedFrame->data, maxOutFrames, inputPlanes, inputFrames);
    if(converted < 0) {
        return errorResult(u"Failed to convert audio for encoder"_s, converted);
    }
    if(converted == 0) {
        return Result::success();
    }

    return queueConvertedAudio(m_convertedFrame.get(), converted);
}

AudioEncoder::Result FFmpegEncoder::prepareFrame(AVFrame* frame, int frames) const
{
    av_frame_unref(frame);

    frame->format      = m_outputSampleFormat;
    frame->sample_rate = m_outputSampleRate;
    frame->nb_samples  = frames;
#if OLD_CHANNEL_LAYOUT
    frame->channel_layout = m_codecContext->channel_layout;
    frame->channels       = m_codecContext->channels;
#else
    if(av_channel_layout_copy(&frame->ch_layout, &m_codecContext->ch_layout) < 0) {
        return Result::failure(u"Failed to copy output channel layout"_s);
    }
#endif

    const int ret = av_frame_get_buffer(frame, 0);
    if(ret < 0) {
        return errorResult(u"Failed to allocate output frame buffer"_s, ret);
    }

    return Result::success();
}

AudioEncoder::Result FFmpegEncoder::queueConvertedAudio(AVFrame* frame, int frames)
{
    if(!m_fifo) {
        frame->nb_samples = frames;
        frame->pts        = m_nextPts;
        m_nextPts += frames;
        return encodeFrame(frame);
    }

    const int requiredSize = av_audio_fifo_size(m_fifo.get()) + frames;
    if(av_audio_fifo_realloc(m_fifo.get(), requiredSize) < 0) {
        return Result::failure(u"Failed to grow encoder audio FIFO"_s);
    }
    if(av_audio_fifo_write(m_fifo.get(), reinterpret_cast<void**>(frame->extended_data), frames) != frames) {
        return Result::failure(u"Failed to queue converted audio"_s);
    }
    return encodeQueuedFrames(false);
}

AudioEncoder::Result FFmpegEncoder::encodeQueuedFrames(bool final)
{
    if(!m_fifo || m_codecContext->frame_size <= 0) {
        return Result::success();
    }

    const int frameSize = m_codecContext->frame_size;
    while(av_audio_fifo_size(m_fifo.get()) >= frameSize || (final && av_audio_fifo_size(m_fifo.get()) > 0)) {
        const int queuedFrames = av_audio_fifo_size(m_fifo.get());
        const int readFrames   = std::min(frameSize, queuedFrames);
        const bool smallLastFrame
            = readFrames < frameSize && (m_codecContext->codec->capabilities & AV_CODEC_CAP_SMALL_LAST_FRAME) != 0;
        const int outputFrames = smallLastFrame ? readFrames : frameSize;

        if(const Result result = prepareFrame(m_frame.get(), outputFrames); !result.ok) {
            return result;
        }

#if OLD_CHANNEL_LAYOUT
        const int channels = m_codecContext->channels;
#else
        const int channels = m_codecContext->ch_layout.nb_channels;
#endif
        if(readFrames < outputFrames) {
            av_samples_set_silence(m_frame->extended_data, 0, outputFrames, channels, m_outputSampleFormat);
        }
        if(av_audio_fifo_read(m_fifo.get(), reinterpret_cast<void**>(m_frame->extended_data), readFrames)
           != readFrames) {
            return Result::failure(u"Failed to read converted audio from FIFO"_s);
        }

        m_frame->pts = m_nextPts;
        m_nextPts += outputFrames;
        if(const Result result = encodeFrame(m_frame.get()); !result.ok) {
            return result;
        }
    }

    return Result::success();
}

AudioEncoder::Result FFmpegEncoder::flushResampler()
{
    while(true) {
        const int maxOutFrames = swr_get_out_samples(m_swr.get(), 0);
        if(maxOutFrames <= 0) {
            return Result::success();
        }
        if(const Result result = prepareFrame(m_convertedFrame.get(), maxOutFrames); !result.ok) {
            return result;
        }

        const int converted = swr_convert(m_swr.get(), m_convertedFrame->data, maxOutFrames, nullptr, 0);
        if(converted < 0) {
            return errorResult(u"Failed to flush encoder resampler"_s, converted);
        }
        if(converted == 0) {
            return Result::success();
        }
        if(const Result result = queueConvertedAudio(m_convertedFrame.get(), converted); !result.ok) {
            return result;
        }
    }
}

AudioEncoder::Result FFmpegEncoder::encodeFrame(AVFrame* frame)
{
    const int ret = avcodec_send_frame(m_codecContext.get(), frame);
    if(ret < 0) {
        return errorResult(u"Failed to send audio frame to encoder"_s, ret);
    }

    return drainPackets();
}

AudioEncoder::Result FFmpegEncoder::drainPackets()
{
    while(true) {
        int ret = avcodec_receive_packet(m_codecContext.get(), m_packet.get());
        if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return Result::success();
        }
        if(ret < 0) {
            return errorResult(u"Failed to receive encoded packet"_s, ret);
        }

        av_packet_rescale_ts(m_packet.get(), m_codecContext->time_base, m_stream->time_base);
        m_packet->stream_index = m_stream->index;

        ret = av_interleaved_write_frame(m_formatContext.get(), m_packet.get());
        av_packet_unref(m_packet.get());
        if(ret < 0) {
            return errorResult(u"Failed to write encoded packet"_s, ret);
        }
    }
}

AudioEncoder::Result FFmpegEncoder::finish()
{
    if(m_finished || !m_codecContext || !m_formatContext) {
        return Result::success();
    }

    if(const Result result = flushResampler(); !result.ok) {
        cleanup();
        return result;
    }
    if(const Result result = encodeQueuedFrames(true); !result.ok) {
        cleanup();
        return result;
    }

    if(const Result result = encodeFrame(nullptr); !result.ok) {
        cleanup();
        return result;
    }

    const int ret = av_write_trailer(m_formatContext.get());
    if(ret < 0) {
        cleanup();
        return errorResult(u"Failed to write output trailer"_s, ret);
    }

    m_finished = true;
    cleanup();
    return Result::success();
}

void FFmpegEncoder::cleanup()
{
    m_swr.reset();
    m_fifo.reset();
    m_frame.reset();
    m_convertedFrame.reset();
    m_packet.reset();
    m_codecContext.reset();
    m_formatContext.reset();

    m_stream             = nullptr;
    m_inputFormat        = {};
    m_inputSampleFormat  = AV_SAMPLE_FMT_NONE;
    m_outputSampleFormat = AV_SAMPLE_FMT_NONE;
    m_outputSampleRate   = 0;
    m_nextPts            = 0;
    m_dither             = false;
}

FFmpegEncoder::~FFmpegEncoder()
{
    cleanup();
}

std::vector<AudioEncoderInfo> FFmpegEncoder::availableEncoders() const
{
    std::vector<AudioEncoderInfo> encoders;

    const auto profiles = builtInProfiles();
    for(const FFmpegProfileDescriptor& descriptor : profiles) {
        QString codecName;
        const AVCodec* codec = findEncoder(descriptor.codecNames, &codecName);
        if(!codec || !hasMuxer(descriptor.containerName, descriptor.extension)) {
            continue;
        }

        AudioEncoderInfo info;
        info.id                  = descriptor.id;
        info.backendId           = u"ffmpeg"_s;
        info.name                = descriptor.name;
        info.description         = description(descriptor);
        info.profile             = makeProfile(descriptor, codecName);
        info.capabilities        = descriptor.capabilities;
        info.estimateBitrateKbps = descriptor.estimateBitrateKbps;

        if(!encoderSupportsOption(codec, "vbr")
           && std::ranges::find(info.capabilities.modes, EncoderMode::ConstrainedVariableBitrate)
                  != info.capabilities.modes.end()) {
            std::erase(info.capabilities.modes, EncoderMode::ConstrainedVariableBitrate);
            std::erase(info.capabilities.modes, EncoderMode::ConstantBitrate);
        }

        info.supportsMetadata = true;
        info.supportsPictures = descriptor.supportsPictures;
        encoders.push_back(std::move(info));
    }

    std::ranges::sort(encoders, [](const AudioEncoderInfo& lhs, const AudioEncoderInfo& rhs) {
        return QString::localeAwareCompare(lhs.name, rhs.name) < 0;
    });

    return encoders;
}
} // namespace Fooyin
