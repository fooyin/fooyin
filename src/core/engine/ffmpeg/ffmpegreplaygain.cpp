/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "ffmpegreplaygain.h"
#include "ffmpegcodec.h"
#include "ffmpeginput.h"
#include "ffmpegutils.h"

#include <core/constants.h>
#include <core/track.h>

#if defined(__GNUG__)
#pragma GCC diagnostic ignored "-Wold-style-cast"
#elif defined(__clang__)
#pragma clang diagnostic ignored "-Wold-style-cast"
#endif

extern "C"
{
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
}

#include <QDebug>
#include <QFile>
#include <QString>

Q_LOGGING_CATEGORY(REPLAYGAIN, "fy.replaygain")

namespace {
struct FilterContextDeleter
{
    void operator()(AVFilterContext* filter) const
    {
        if(filter) {
            avfilter_free(filter);
        }
    }
};
using FilterContextPtr = std::unique_ptr<AVFilterContext, FilterContextDeleter>;

struct FilterGraphDeleter
{
    void operator()(AVFilterGraph* graph) const
    {
        if(graph) {
            avfilter_graph_free(&graph);
        }
    }
};
using FilterGraphPtr = std::unique_ptr<AVFilterGraph, FilterGraphDeleter>;

struct FilterInOutDeleter
{
    void operator()(AVFilterInOut* inout) const
    {
        if(inout) {
            avfilter_inout_free(&inout);
        }
    }
};
using FilterInOutPtr = std::unique_ptr<AVFilterInOut, FilterInOutDeleter>;

constexpr auto FrameFlags = AV_BUFFERSRC_FLAG_KEEP_REF | AV_BUFFERSRC_FLAG_NO_CHECK_FORMAT | AV_BUFFERSRC_FLAG_PUSH;

bool finishFilter(AVFilterContext* filter)
{
    return av_buffersrc_add_frame_flags(filter, nullptr, FrameFlags) >= 0;
}
} // namespace

namespace Fooyin {
using Utils::printError;

struct ReplayGainResult
{
    double gain;
    double peak;
};

ReplayGainResult extractRGValues(AVFilterGraph* graph)
{
    ReplayGainResult result{};

    av_opt_get_double(graph->filters[1], "integrated", AV_OPT_SEARCH_CHILDREN, &result.gain);
    av_opt_get_double(graph->filters[1], "true_peak", AV_OPT_SEARCH_CHILDREN, &result.peak);
    result.gain = -18.0 - result.gain; // TODO - newer standard uses 23.0 as reference. make configurable?
    result.peak = std::pow(10, result.peak / 20.0);

    return result;
}

struct ReplayGainFilter
{
    AVFilterContext* filterContext; // FIXME FilterContextPtr
    FilterGraphPtr filterGraph;
};

class FFmpegReplayGainPrivate
{
public:
    explicit FFmpegReplayGainPrivate(MusicLibrary* library);

    bool setupTrack(const Track& track, ReplayGainFilter& filter);
    ReplayGainResult handleTrack(const Track& track, bool inAlbum);
    void handleAlbum(const TrackList& album);

    std::unique_ptr<QIODevice> m_file;
    std::unique_ptr<FFmpegDecoder> m_decoder;
    AudioFormat m_format;

    MusicLibrary* m_library;

    ReplayGainFilter m_albumFilter;
    ReplayGainFilter m_trackFilter;
};

FFmpegReplayGain::FFmpegReplayGain(MusicLibrary* library, QObject* parent)
    : Worker{parent}
    , p{std::make_unique<FFmpegReplayGainPrivate>(library)}
{ }

FFmpegReplayGain::~FFmpegReplayGain() = default;

FFmpegReplayGainPrivate::FFmpegReplayGainPrivate(MusicLibrary* library)
    : m_file{nullptr}
    , m_decoder{nullptr}
    , m_format{}
    , m_library{library}
    , m_albumFilter{}
    , m_trackFilter{}
{ }

void FFmpegReplayGain::calculate(const TrackList& tracks, bool asAlbum)
{
    setState(Running);

    if(asAlbum) {
        p->handleAlbum(tracks);
    }
    else {
        for(auto track : tracks) {
            if(p->setupTrack(track, p->m_trackFilter)) {
                const auto result = p->handleTrack(track, false);
                track.setRGTrackGain(static_cast<float>(result.gain));
                track.setRGTrackPeak(static_cast<float>(result.peak));
                p->m_library->writeTrackMetadata({track});
            }
        }
    }

    if(p->m_decoder) {
        p->m_decoder->stop();
    }

    emit finished();
    setState(Idle);
}

ReplayGainFilter initialiseRGFilter(const AudioFormat& format)
{
    int rc{0};
    ReplayGainFilter filter{};

    AVFilterGraph* filterGraph = avfilter_graph_alloc();
    if(!filterGraph) {
        qCWarning(REPLAYGAIN, "Unable to allocate filter graph");
        return filter;
    }

    const auto sampleFmt  = format.sampleFormat();
    const auto sampleRate = format.sampleRate();

    const auto sampleFmtName
        = std::string{av_get_sample_fmt_name(Utils::sampleFormat(sampleFmt, format.sampleFormatIsPlanar()))};
    const auto args = QString{QStringLiteral("time_base=%1/%2:sample_rate=%2:sample_fmt=%3:channel_layout=0x%4")}
                          .arg(1)
                          .arg(sampleRate)
                          .arg(QString::fromStdString(sampleFmtName))
                          .arg(AV_CH_LAYOUT_STEREO, 0, 16)
                          .toStdString();

    // Allocate and configure filter
    AVFilterContext* filterCtx{nullptr};
    rc = avfilter_graph_create_filter(&filterCtx, avfilter_get_by_name("abuffer"), "in", args.c_str(), nullptr,
                                      filterGraph);
    if(rc < 0) {
        printError(rc);
        return filter;
    }
    filter.filterContext = filterCtx;

    // Add ebur128 graph
    // const FilterInOutPtr outputsPtr{avfilter_inout_alloc()}; // FIXME - freed too early
    AVFilterInOut* outputs = avfilter_inout_alloc();
    outputs->name          = av_strdup("in");
    outputs->filter_ctx    = filterCtx;
    outputs->pad_idx       = 0;
    outputs->next          = nullptr;
    AVFilterInOut* inputs  = nullptr;
    rc = avfilter_graph_parse_ptr(filterGraph, "ebur128=peak=true:framelog=quiet,anullsink", &inputs, &outputs,
                                  nullptr);
    if(rc < 0) {
        printError(rc);
        return filter;
    }

    rc = avfilter_graph_config(filterGraph, nullptr);
    if(rc < 0) {
        printError(rc);
        return filter;
    }

    filter.filterGraph.reset(filterGraph);
    return filter;
}

bool FFmpegReplayGainPrivate::setupTrack(const Track& track, ReplayGainFilter& filter)
{
    if(m_decoder) {
        m_decoder->stop();
    }

    if(!track.isValid()) {
        return false;
    }

    if(!track.isInArchive() && !QFile::exists(track.filepath())) {
        return false;
    }

    AudioSource source;
    source.filepath = track.filepath();
    if(!track.isInArchive()) {
        m_file = std::make_unique<QFile>(track.filepath());
        if(!m_file->open(QIODevice::ReadOnly)) {
            qCWarning(REPLAYGAIN) << "Failed to open" << track.filepath();
            return false;
        }
        source.device = m_file.get();
    }

    m_decoder         = std::make_unique<FFmpegDecoder>();
    const auto format = m_decoder->init(source, track, AudioDecoder::NoSeeking | AudioDecoder::NoInfiniteLooping);
    if(!format) {
        return false;
    }

    m_format = format.value();
    filter   = initialiseRGFilter(m_format);
    return true;
}

ReplayGainResult FFmpegReplayGainPrivate::handleTrack(const Track& track, bool inAlbum)
{
    int rc{0};
    const FormatContextPtr formatCtxPtr{avformat_alloc_context()};
    auto* formatCtx = formatCtxPtr.get();

    avformat_open_input(&formatCtx, track.filepath().toUtf8().constData(), nullptr, nullptr);
    avformat_find_stream_info(formatCtx, nullptr);

#if(LIBAVFORMAT_VERSION_INT > AV_VERSION_INT(58, 79, 100))
    const AVCodec* decoder{};
#else
    AVCodec* decoder{};
#endif
    AVStream* stream{};
    int streamIndex = -1;
    for(unsigned int i = 0; i < formatCtx->nb_streams; i++) {
        if(formatCtx->streams[i]->codecpar->codec_type != AVMEDIA_TYPE_AUDIO) {
            continue;
        }
        streamIndex = av_find_best_stream(formatCtx, AVMEDIA_TYPE_AUDIO, i, -1, &decoder, 0);
        if(streamIndex < 0) {
            return {};
        }
        stream = formatCtx->streams[streamIndex];
        break;
    }
    const CodecContextPtr decoderCtxPtr{avcodec_alloc_context3(decoder)};
    auto* decoderCtx = decoderCtxPtr.get();
    avcodec_parameters_to_context(decoderCtx, stream->codecpar);
    avcodec_open2(decoderCtx, decoder, nullptr);

    const FramePtr framePtr{av_frame_alloc()};
    const PacketPtr packetPtr{av_packet_alloc()};
    auto* frame  = framePtr.get();
    auto* packet = packetPtr.get();

    while(rc == 0) {
        rc = av_read_frame(formatCtx, packet);
        if(rc < 0) {
            break;
        }

        if(packet->stream_index != streamIndex) {
            av_packet_unref(packet);
            continue;
        }

        rc = avcodec_send_packet(decoderCtx, packet);
        if(rc < 0) {
            printError(rc);
            break;
        }

        while(rc >= 0) {
            rc = avcodec_receive_frame(decoderCtx, frame);
            if(rc == AVERROR_EOF || rc == AVERROR(EAGAIN)) {
                rc = 0;
                break;
            }
            if(rc < 0) {
                printError(rc);
                break;
            }
            rc = av_buffersrc_add_frame_flags(m_trackFilter.filterContext, frame, FrameFlags);
            if(rc < 0) {
                printError(rc);
                break;
            }
            if(inAlbum) {
                rc = av_buffersrc_add_frame_flags(m_albumFilter.filterContext, frame, FrameFlags);
                if(rc < 0) {
                    printError(rc);
                    break;
                }
            }
            av_frame_unref(frame);
        }

        av_packet_unref(packet);
    }

    if(!finishFilter(m_trackFilter.filterContext)) {
        return {};
    }

    return extractRGValues(m_trackFilter.filterGraph.get());
}

void FFmpegReplayGainPrivate::handleAlbum(const TrackList& album)
{
    // FIXME
    if(!setupTrack(album[0], m_albumFilter)) {
        return;
    }

    if(!m_albumFilter.filterContext || !m_albumFilter.filterGraph) {
        return;
    }

    std::vector<ReplayGainResult> trackResults;

    for(auto track : album) {
        if(setupTrack(track, m_trackFilter)) {
            trackResults.emplace_back(handleTrack(track, true));
        }
    }

    if(!finishFilter(m_albumFilter.filterContext)) {
        return;
    }

    const auto albumResult = extractRGValues(m_albumFilter.filterGraph.get());

    for(size_t i = 0; auto track : album) {
        track.setRGTrackGain(static_cast<float>(trackResults[i].gain));
        track.setRGTrackPeak(static_cast<float>(trackResults[i].peak));
        track.setRGAlbumPeak(static_cast<float>(albumResult.peak));
        track.setRGAlbumGain(static_cast<float>(albumResult.gain));
        m_library->writeTrackMetadata({track});
        i++;
    }
}
} // namespace Fooyin
