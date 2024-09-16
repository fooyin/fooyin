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
#include "ffmpegstream.h"
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

struct ReplayGainResult
{
    double gain;
    double peak;
};

struct ReplayGainFilter
{
    AVFilterContext* filterContext;
    AVFilterInOut* filterOutput;
    FilterGraphPtr filterGraph;
};

constexpr auto FrameFlags = AV_BUFFERSRC_FLAG_KEEP_REF | AV_BUFFERSRC_FLAG_NO_CHECK_FORMAT | AV_BUFFERSRC_FLAG_PUSH;

bool finishFilter(AVFilterContext* filter)
{
    return av_buffersrc_add_frame_flags(filter, nullptr, FrameFlags) >= 0;
}

ReplayGainResult extractRGValues(AVFilterGraph* graph)
{
    ReplayGainResult result{};

    av_opt_get_double(graph->filters[1], "integrated", AV_OPT_SEARCH_CHILDREN, &result.gain);
    av_opt_get_double(graph->filters[1], "true_peak", AV_OPT_SEARCH_CHILDREN, &result.peak);
    result.gain = -18.0 - result.gain; // TODO - newer standard uses 23.0 as reference. make configurable?
    result.peak = std::pow(10, result.peak / 20.0);

    return result;
}
} // namespace

namespace Fooyin {
class FFmpegReplayGainPrivate
{
public:
    explicit FFmpegReplayGainPrivate(MusicLibrary* library);

    [[nodiscard]] bool setupTrack(const Track& track, ReplayGainFilter& filter);
    [[nodiscard]] ReplayGainResult handleTrack(bool inAlbum) const;
    void handleAlbum(const TrackList& album);

    AudioFormat m_format;
    FormatContextPtr m_formatCtx;
    CodecContextPtr m_decoderCtx;
    ReplayGainFilter m_albumFilter;
    ReplayGainFilter m_trackFilter;
    int m_streamIndex{-1};

    MusicLibrary* m_library;
};

FFmpegReplayGain::FFmpegReplayGain(MusicLibrary* library, QObject* parent)
    : Worker{parent}
    , p{std::make_unique<FFmpegReplayGainPrivate>(library)}
{ }

FFmpegReplayGain::~FFmpegReplayGain() = default;

FFmpegReplayGainPrivate::FFmpegReplayGainPrivate(MusicLibrary* library)
    : m_albumFilter{}
    , m_trackFilter{}
    , m_library{library}
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
                const auto result = p->handleTrack(false);
                track.setRGTrackGain(static_cast<float>(result.gain));
                track.setRGTrackPeak(static_cast<float>(result.peak));
                p->m_library->writeTrackMetadata({track});
            }
        }
    }

    if(p->m_trackFilter.filterContext) {
        avfilter_free(p->m_trackFilter.filterContext);
    }
    if(p->m_albumFilter.filterContext) {
        avfilter_free(p->m_albumFilter.filterContext);
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
    filter.filterGraph.reset(filterGraph);

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
        Utils::printError(rc);
        return filter;
    }
    filter.filterContext = filterCtx;

    // Add ebur128 graph
    AVFilterInOut* outputs = avfilter_inout_alloc();
    if(!outputs) {
        qCWarning(REPLAYGAIN, "Unable to allocate filter output");
        return filter;
    }
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = filterCtx;
    outputs->pad_idx    = 0;
    outputs->next       = nullptr;
    filter.filterOutput = outputs;

    AVFilterInOut* inputs = nullptr;
    rc = avfilter_graph_parse_ptr(filterGraph, "ebur128=peak=true:framelog=quiet,anullsink", &inputs, &outputs,
                                  nullptr);
    if(rc < 0) {
        Utils::printError(rc);
        return filter;
    }

    rc = avfilter_graph_config(filterGraph, nullptr);
    if(rc < 0) {
        Utils::printError(rc);
        return filter;
    }

    return filter;
}

bool FFmpegReplayGainPrivate::setupTrack(const Track& track, ReplayGainFilter& filter)
{
    int rc{0};
    m_formatCtx.reset(avformat_alloc_context());
    auto* formatCtx = m_formatCtx.get();

    if(rc = avformat_open_input(&formatCtx, track.filepath().toUtf8().constData(), nullptr, nullptr); rc < 0) {
        return false;
    }

    if(rc = avformat_find_stream_info(formatCtx, nullptr); rc < 0) {
        return false;
    }

#if(LIBAVFORMAT_VERSION_INT > AV_VERSION_INT(58, 79, 100))
    const AVCodec* decoder{};
#else
    AVCodec* decoder{};
#endif
    m_decoderCtx.reset(avcodec_alloc_context3(decoder));
    auto* decoderCtx = m_decoderCtx.get();

    const auto stream = findAudioStream(formatCtx);
    if(!stream.isValid()) {
        return false;
    }

    m_streamIndex = av_find_best_stream(formatCtx, AVMEDIA_TYPE_AUDIO, stream.index(), -1, &decoder, 0);
    if(m_streamIndex < 0) {
        return false;
    }

    if(rc = avcodec_parameters_to_context(decoderCtx, stream.avStream()->codecpar); rc < 0) {
        Utils::printError(rc);
        return false;
    }

    if(rc = avcodec_open2(decoderCtx, decoder, nullptr); rc < 0) {
        Utils::printError(rc);
        return false;
    }

    m_format = Utils::audioFormatFromCodec(stream.avStream()->codecpar);
    filter   = initialiseRGFilter(m_format);

    return true;
}

ReplayGainResult FFmpegReplayGainPrivate::handleTrack(bool inAlbum) const
{
    int rc{0};
    const FramePtr framePtr{av_frame_alloc()};
    const PacketPtr packetPtr{av_packet_alloc()};
    auto* frame  = framePtr.get();
    auto* packet = packetPtr.get();

    while(rc == 0) {
        rc = av_read_frame(m_formatCtx.get(), packet);
        if(rc < 0) {
            break;
        }

        if(packet->stream_index != m_streamIndex) {
            av_packet_unref(packet);
            continue;
        }

        rc = avcodec_send_packet(m_decoderCtx.get(), packet);
        if(rc < 0) {
            Utils::printError(rc);
            break;
        }

        while(rc >= 0) {
            rc = avcodec_receive_frame(m_decoderCtx.get(), frame);
            if(rc == AVERROR_EOF || rc == AVERROR(EAGAIN)) {
                rc = 0;
                break;
            }
            if(rc < 0) {
                Utils::printError(rc);
                break;
            }
            rc = av_buffersrc_add_frame_flags(m_trackFilter.filterContext, frame, FrameFlags);
            if(rc < 0) {
                Utils::printError(rc);
                break;
            }
            if(inAlbum) {
                rc = av_buffersrc_add_frame_flags(m_albumFilter.filterContext, frame, FrameFlags);
                if(rc < 0) {
                    Utils::printError(rc);
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
            trackResults.emplace_back(handleTrack(true));
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
