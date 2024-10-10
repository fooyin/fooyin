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

#include "ffmpegscanner.h"

#include <core/constants.h>
#include <core/coresettings.h>
#include <core/engine/ffmpeg/ffmpeginput.h>
#include <core/engine/ffmpeg/ffmpegutils.h>
#include <core/scripting/scriptparser.h>
#include <core/track.h>
#include <utils/settings/settingsmanager.h>

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
#include <QFuture>
#include <QFutureWatcher>
#include <QLoggingCategory>
#include <QString>
#include <QtConcurrentMap>

#include <ranges>

Q_LOGGING_CATEGORY(REPLAYGAIN, "fy.ffmpegscanner")

constexpr auto FrameFlags   = AV_BUFFERSRC_FLAG_KEEP_REF | AV_BUFFERSRC_FLAG_NO_CHECK_FORMAT | AV_BUFFERSRC_FLAG_PUSH;
constexpr auto DecoderFlags = Fooyin::AudioDecoder::NoSeeking | Fooyin::AudioDecoder::NoLooping;

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
    double gain{Fooyin::Constants::InvalidGain};
    double peak{Fooyin::Constants::InvalidPeak};
};

struct ReplayGainFilter
{
    AVFilterContext* filterContext{nullptr};
    AVFilterInOut* filterOutput{nullptr};
    FilterGraphPtr filterGraph;
};

struct FFmpegContext
{
    explicit FFmpegContext(bool truePeak_)
        : truePeak{truePeak_}
    { }

    ~FFmpegContext()
    {
        decoder.stop();
        if(trackFilter.filterContext) {
            avfilter_free(trackFilter.filterContext);
            trackFilter.filterContext = nullptr;
        }
        if(albumFilter.filterContext) {
            avfilter_free(albumFilter.filterContext);
            albumFilter.filterContext = nullptr;
        }
    }

    Fooyin::AudioFormat format;
    std::unique_ptr<QFile> file;
    Fooyin::FFmpegDecoder decoder;
    ReplayGainFilter albumFilter;
    ReplayGainFilter trackFilter;
    bool truePeak{false};
};

bool finishFilter(AVFilterContext* filter)
{
    return av_buffersrc_add_frame_flags(filter, nullptr, FrameFlags) >= 0;
}

ReplayGainResult extractRGValues(AVFilterGraph* graph, bool truePeak)
{
    ReplayGainResult result;

    av_opt_get_double(graph->filters[1], "integrated", AV_OPT_SEARCH_CHILDREN, &result.gain);
    av_opt_get_double(graph->filters[1], truePeak ? "true_peak" : "sample_peak", AV_OPT_SEARCH_CHILDREN, &result.peak);
    result.gain = -18.0 - result.gain; // TODO - newer standard uses 23.0 as reference. make configurable?
    result.peak = std::pow(10, result.peak / 20.0);

    return result;
}

ReplayGainFilter initialiseRGFilter(const Fooyin::AudioFormat& format, bool truePeak)
{
    int rc{0};
    ReplayGainFilter filter;

    AVFilterGraph* filterGraph = avfilter_graph_alloc();
    if(!filterGraph) {
        qCWarning(REPLAYGAIN, "Unable to allocate filter graph");
        return filter;
    }
    filter.filterGraph.reset(filterGraph);

    const auto sampleFmt  = format.sampleFormat();
    const auto sampleRate = format.sampleRate();

    const auto sampleFmtName
        = std::string{av_get_sample_fmt_name(Fooyin::Utils::sampleFormat(sampleFmt, format.sampleFormatIsPlanar()))};
    const auto args = QString{QStringLiteral("time_base=%1/%2:sample_rate=%2:sample_fmt=%3:channel_layout=0x%4")}
                          .arg(1)
                          .arg(sampleRate)
                          .arg(QString::fromStdString(sampleFmtName))
                          .arg(AV_CH_LAYOUT_STEREO, 0, 16);

    // Allocate and configure filter
    AVFilterContext* filterCtx{nullptr};
    rc = avfilter_graph_create_filter(&filterCtx, avfilter_get_by_name("abuffer"), "in", args.toUtf8().constData(),
                                      nullptr, filterGraph);
    if(rc < 0) {
        Fooyin::Utils::printError(rc);
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
    const auto filterParams
        = QString{QStringLiteral("ebur128=peak=%1:framelog=quiet,anullsink")}.arg(truePeak ? u"true" : u"sample");
    rc = avfilter_graph_parse_ptr(filterGraph, filterParams.toUtf8().constData(), &inputs, &outputs, nullptr);
    if(rc < 0) {
        Fooyin::Utils::printError(rc);
        return filter;
    }

    rc = avfilter_graph_config(filterGraph, nullptr);
    if(rc < 0) {
        Fooyin::Utils::printError(rc);
        return filter;
    }

    return filter;
}

bool setupTrack(FFmpegContext& context, const Fooyin::Track& track, ReplayGainFilter& filter)
{
    if(track.isInArchive()) {
        return false;
    }

    Fooyin::AudioSource source;
    context.file = std::make_unique<QFile>(track.filepath());
    if(!context.file->open(QIODevice::ReadOnly)) {
        return false;
    }
    source.device   = context.file.get();
    source.filepath = track.filepath();

    const auto format = context.decoder.init(source, track, DecoderFlags);
    if(!format) {
        return false;
    }

    context.format = format.value();
    filter         = initialiseRGFilter(context.format, context.truePeak);
    context.decoder.start();

    return true;
}

ReplayGainResult handleTrack(FFmpegContext& context, bool inAlbum)
{
    int rc{0};
    Fooyin::Frame frame;
    while((frame = context.decoder.readFrame()).isValid()) {
        rc = av_buffersrc_add_frame_flags(context.trackFilter.filterContext, frame.avFrame(), FrameFlags);
        if(rc < 0) {
            Fooyin::Utils::printError(rc);
            break;
        }
        if(inAlbum) {
            rc = av_buffersrc_add_frame_flags(context.albumFilter.filterContext, frame.avFrame(), FrameFlags);
            if(rc < 0) {
                Fooyin::Utils::printError(rc);
                break;
            }
        }
    }

    if(!finishFilter(context.trackFilter.filterContext)) {
        return {};
    }

    return extractRGValues(context.trackFilter.filterGraph.get(), context.truePeak);
}
} // namespace

namespace Fooyin::RGScanner {
class FFmpegReplayGainPrivate
{
public:
    explicit FFmpegReplayGainPrivate(FFmpegScanner* self);

    void scanAlbum(FFmpegContext& context, TrackList& tracks) const;

    FFmpegScanner* m_self;

    TrackList m_tracks;
    TrackList m_scannedTracks;
    ScriptParser m_parser;
    QFutureWatcher<void>* m_future{nullptr};
};

FFmpegReplayGainPrivate::FFmpegReplayGainPrivate(FFmpegScanner* self)
    : m_self{self}
{ }

void FFmpegReplayGainPrivate::scanAlbum(FFmpegContext& context, TrackList& tracks) const
{
    if(!setupTrack(context, tracks.front(), context.albumFilter)) {
        return;
    }

    if(!context.albumFilter.filterContext || !context.albumFilter.filterGraph) {
        return;
    }

    for(Track& track : tracks) {
        if(!m_self->mayRun()) {
            return;
        }
        emit m_self->startingCalculation(track.prettyFilepath());

        if(setupTrack(context, track, context.trackFilter)) {
            const ReplayGainResult trackResult = handleTrack(context, true);
            track.setRGTrackGain(static_cast<float>(trackResult.gain));
            track.setRGTrackPeak(static_cast<float>(trackResult.peak));
        }
    }

    if(!finishFilter(context.albumFilter.filterContext)) {
        return;
    }

    const auto albumResult = extractRGValues(context.albumFilter.filterGraph.get(), context.truePeak);

    for(Track& track : tracks) {
        track.setRGAlbumPeak(static_cast<float>(albumResult.peak));
        track.setRGAlbumGain(static_cast<float>(albumResult.gain));
    }
}

FFmpegScanner::FFmpegScanner(QObject* parent)
    : RGWorker{parent}
    , p{std::make_unique<FFmpegReplayGainPrivate>(this)}
{ }

void FFmpegScanner::closeThread()
{
    RGWorker::closeThread();
    QMetaObject::invokeMethod(p->m_future, [this]() {
        p->m_future->cancel();
        p->m_future->waitForFinished();
    });
}

FFmpegScanner::~FFmpegScanner() = default;

void FFmpegScanner::calculatePerTrack(const TrackList& tracks, bool truePeak)
{
    setState(Running);

    p->m_future = new QFutureWatcher<void>(this);

    p->m_tracks        = tracks;
    p->m_scannedTracks = tracks;

    QObject::connect(p->m_future, &QFutureWatcher<void>::progressValueChanged, this, [this](const int val) {
        if(val >= 0 && std::cmp_less(val, p->m_tracks.size())) {
            emit startingCalculation(p->m_tracks.at(val).prettyFilepath());
        }
    });

    auto future = QtConcurrent::map(p->m_scannedTracks, [truePeak](Track& track) {
        FFmpegContext context{truePeak};

        if(setupTrack(context, track, context.trackFilter)) {
            const ReplayGainResult result = handleTrack(context, false);
            track.setRGTrackGain(static_cast<float>(result.gain));
            track.setRGTrackPeak(static_cast<float>(result.peak));
        }
    });

    p->m_future->setFuture(future);

    future.then(this, [this]() {
        if(mayRun()) {
            emit calculationFinished(p->m_scannedTracks);
        }

        emit finished();
        setState(Idle);
    });
}

void FFmpegScanner::calculateAsAlbum(const TrackList& tracks, bool truePeak)
{
    setState(Running);

    FFmpegContext context{truePeak};

    TrackList scannedTracks{tracks};
    p->scanAlbum(context, scannedTracks);

    if(mayRun()) {
        emit calculationFinished(scannedTracks);
        emit finished();
    }

    setState(Idle);
}

void FFmpegScanner::calculateByAlbumTags(const TrackList& tracks, const QString& groupScript, bool truePeak)
{
    setState(Running);

    std::unordered_map<QString, TrackList> albums;
    for(const auto& track : tracks) {
        const QString album = p->m_parser.evaluate(groupScript, track);
        albums[album].push_back(track);
    }

    TrackList scannedTracks;

    FFmpegContext context{truePeak};

    for(TrackList& album : albums | std::views::values) {
        if(!mayRun()) {
            return;
        }
        p->scanAlbum(context, album);
        scannedTracks.insert(scannedTracks.end(), album.cbegin(), album.cend());
    }

    if(mayRun()) {
        emit calculationFinished(scannedTracks);
        emit finished();
    }

    setState(Idle);
}
} // namespace Fooyin::RGScanner
