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
#include "ffmpeginput.h"
#include "ffmpegutils.h"

#include <core/constants.h>
#include <core/coresettings.h>
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
#include <QString>

#include <ranges>

Q_LOGGING_CATEGORY(REPLAYGAIN, "fy.replaygain")

constexpr auto FrameFlags = AV_BUFFERSRC_FLAG_KEEP_REF | AV_BUFFERSRC_FLAG_NO_CHECK_FORMAT | AV_BUFFERSRC_FLAG_PUSH;

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
} // namespace

namespace Fooyin {
class FFmpegReplayGainPrivate
{
public:
    explicit FFmpegReplayGainPrivate(FFmpegReplayGain* self, SettingsManager* settings);

    [[nodiscard]] bool setupTrack(const Track& track, ReplayGainFilter& filter);
    [[nodiscard]] ReplayGainResult handleTrack(bool inAlbum);
    void scanAlbum(TrackList& tracks);

    void cleanup();

    FFmpegReplayGain* m_self;
    SettingsManager* m_settings;

    ScriptParser m_parser;
    AudioFormat m_format;
    std::unique_ptr<QFile> m_file;
    FFmpegDecoder m_decoder;
    ReplayGainFilter m_albumFilter;
    ReplayGainFilter m_trackFilter;
};

FFmpegReplayGainPrivate::FFmpegReplayGainPrivate(FFmpegReplayGain* self, SettingsManager* settings)
    : m_self{self}
    , m_settings{settings}
    , m_decoder{m_settings}
{ }

bool FFmpegReplayGainPrivate::setupTrack(const Track& track, ReplayGainFilter& filter)
{
    if(track.isInArchive()) {
        return false;
    }

    AudioSource source;
    m_file = std::make_unique<QFile>(track.filepath());
    if(!m_file->open(QIODevice::ReadOnly)) {
        return false;
    }
    source.device   = m_file.get();
    source.filepath = track.filepath();

    const auto format = m_decoder.init(source, track, AudioDecoder::NoSeeking | AudioDecoder::NoLooping);
    if(!format) {
        return false;
    }

    m_format = format.value();
    filter   = initialiseRGFilter(m_format, m_settings->value<Settings::Core::RGTruePeak>());
    m_decoder.start();

    return true;
}

ReplayGainResult FFmpegReplayGainPrivate::handleTrack(bool inAlbum)
{
    int rc{0};
    Frame frame;
    while((frame = m_decoder.readFrame()).isValid()) {
        rc = av_buffersrc_add_frame_flags(m_trackFilter.filterContext, frame.avFrame(), FrameFlags);
        if(rc < 0) {
            Utils::printError(rc);
            break;
        }
        if(inAlbum) {
            rc = av_buffersrc_add_frame_flags(m_albumFilter.filterContext, frame.avFrame(), FrameFlags);
            if(rc < 0) {
                Utils::printError(rc);
                break;
            }
        }
    }

    if(!finishFilter(m_trackFilter.filterContext)) {
        return {};
    }

    return extractRGValues(m_trackFilter.filterGraph.get(), m_settings->value<Settings::Core::RGTruePeak>());
}

void FFmpegReplayGainPrivate::scanAlbum(TrackList& tracks)
{
    if(!setupTrack(tracks.front(), m_albumFilter)) {
        return;
    }

    if(!m_albumFilter.filterContext || !m_albumFilter.filterGraph) {
        return;
    }

    for(Track& track : tracks) {
        if(!m_self->mayRun()) {
            return;
        }
        if(setupTrack(track, m_trackFilter)) {
            const ReplayGainResult trackResult = handleTrack(true);
            track.setRGTrackGain(static_cast<float>(trackResult.gain));
            track.setRGTrackPeak(static_cast<float>(trackResult.peak));
            emit m_self->rgCalculated(track.prettyFilepath());
        }
    }

    if(!finishFilter(m_albumFilter.filterContext)) {
        return;
    }

    const auto albumResult
        = extractRGValues(m_albumFilter.filterGraph.get(), m_settings->value<Settings::Core::RGTruePeak>());

    for(Track& track : tracks) {
        track.setRGAlbumPeak(static_cast<float>(albumResult.peak));
        track.setRGAlbumGain(static_cast<float>(albumResult.gain));
    }
}

void FFmpegReplayGainPrivate::cleanup()
{
    m_decoder.stop();
    if(m_trackFilter.filterContext) {
        avfilter_free(m_trackFilter.filterContext);
        m_trackFilter.filterContext = nullptr;
    }
    if(m_albumFilter.filterContext) {
        avfilter_free(m_albumFilter.filterContext);
        m_albumFilter.filterContext = nullptr;
    }
}

FFmpegReplayGain::FFmpegReplayGain(SettingsManager* settings, QObject* parent)
    : Worker{parent}
    , p{std::make_unique<FFmpegReplayGainPrivate>(this, settings)}
{ }

FFmpegReplayGain::~FFmpegReplayGain() = default;

void FFmpegReplayGain::calculatePerTrack(const TrackList& tracks)
{
    setState(Running);

    TrackList scannedTracks{tracks};

    for(Track& track : scannedTracks) {
        if(!mayRun()) {
            return;
        }
        if(p->setupTrack(track, p->m_trackFilter)) {
            const ReplayGainResult result = p->handleTrack(false);
            track.setRGTrackGain(static_cast<float>(result.gain));
            track.setRGTrackPeak(static_cast<float>(result.peak));
            emit rgCalculated(track.prettyFilepath());
        }
    }

    p->cleanup();

    if(mayRun()) {
        emit calculationFinished(scannedTracks);
        emit finished();
    }

    setState(Idle);
}

void FFmpegReplayGain::calculateAsAlbum(const TrackList& tracks)
{
    setState(Running);

    TrackList scannedTracks{tracks};
    p->scanAlbum(scannedTracks);
    p->cleanup();

    if(mayRun()) {
        emit calculationFinished(scannedTracks);
        emit finished();
    }

    setState(Idle);
}

void FFmpegReplayGain::calculateByAlbumTags(const TrackList& tracks)
{
    setState(Running);

    const auto groupScript = p->m_settings->value<Settings::Core::RGAlbumGroupScript>();

    std::unordered_map<QString, TrackList> albums;
    for(const auto& track : tracks) {
        const QString album = p->m_parser.evaluate(groupScript, track);
        albums[album].push_back(track);
    }

    TrackList scannedTracks;

    for(TrackList& album : albums | std::views::values) {
        if(!mayRun()) {
            return;
        }
        p->scanAlbum(album);
        scannedTracks.insert(scannedTracks.end(), album.cbegin(), album.cend());
    }

    p->cleanup();

    if(mayRun()) {
        emit calculationFinished(scannedTracks);
        emit finished();
    }

    setState(Idle);
}
} // namespace Fooyin
