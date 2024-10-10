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

#include "ebur128scanner.h"

#include <core/constants.h>
#include <core/engine/audioconverter.h>

#include <QFile>
#include <QFuture>
#include <QFutureWatcher>
#include <QLoggingCategory>
#include <QString>
#include <QtConcurrentMap>

Q_LOGGING_CATEGORY(EBUR128, "fy.ebur128")

constexpr auto ReferenceLUFS  = -18;
constexpr auto BufferSize     = 10240;
constexpr auto SingleAlbumKey = "Album";

namespace Fooyin::RGScanner {
Ebur128Scanner::Ebur128Scanner(std::shared_ptr<AudioLoader> audioLoader, QObject* parent)
    : RGWorker{parent}
    , m_audioLoader{std::move(audioLoader)}
    , m_watcher{nullptr}
    , m_decoder{nullptr}
    , m_runningWatchers{0}
{ }

void Ebur128Scanner::closeThread()
{
    RGWorker::closeThread();

    auto cancelFuture = [](QFutureWatcher<void>* watcher) {
        if(watcher) {
            watcher->cancel();
            watcher->waitForFinished();
        }
    };

    cancelFuture(m_watcher);
    for(const auto& [_, watcher] : m_albumWatchers) {
        cancelFuture(watcher);
    }
}

void Ebur128Scanner::calculatePerTrack(const TrackList& tracks, bool truePeak)
{
    setState(Running);

    m_watcher       = new QFutureWatcher<void>(this);
    m_tracks        = tracks;
    m_scannedTracks = tracks;

    QObject::connect(m_watcher, &QFutureWatcher<void>::progressValueChanged, this, [this](const int val) {
        if(val >= 0 && std::cmp_less(val, m_tracks.size())) {
            emit startingCalculation(m_tracks.at(val).prettyFilepath());
        }
    });

    auto future = QtConcurrent::map(m_scannedTracks, [this, truePeak](Track& track) { scanTrack(track, truePeak); });

    m_watcher->setFuture(future);
    m_runningWatchers.fetch_add(1, std::memory_order_acquire);

    future.then(this, [this]() {
        if(mayRun()) {
            emit calculationFinished(m_scannedTracks);
        }
        if(m_runningWatchers.fetch_sub(1, std::memory_order_release) <= 1) {
            emit finished();
        }
        setState(Idle);
    });
}

void Ebur128Scanner::calculateAsAlbum(const TrackList& tracks, bool truePeak)
{
    setState(Running);

    m_watcher       = new QFutureWatcher<void>(this);
    m_tracks        = tracks;
    m_scannedTracks = tracks;

    QObject::connect(m_watcher, &QFutureWatcher<void>::progressValueChanged, this, [this](const int val) {
        if(val >= 0 && std::cmp_less(val, m_tracks.size())) {
            emit startingCalculation(m_tracks.at(val).prettyFilepath());
        }
    });

    auto future = QtConcurrent::map(m_scannedTracks, [this, truePeak](Track& track) {
        scanTrack(track, truePeak, QString::fromLatin1(SingleAlbumKey));
    });

    m_watcher->setFuture(future);
    m_runningWatchers.fetch_add(1, std::memory_order_acquire);

    future.then(this, [this]() {
        const auto albumState = m_albumStates.find(QString::fromLatin1(SingleAlbumKey));
        if(albumState != m_albumStates.cend()) {
            const auto& trackStates = albumState->second;
            std::vector<ebur128_state*> states;
            std::ranges::transform(trackStates, std::back_inserter(states),
                                   [](const auto& state) { return state.get(); });

            double albumGain{Constants::InvalidGain};
            if(ebur128_loudness_global_multiple(states.data(), states.size(), &albumGain) == EBUR128_SUCCESS) {
                albumGain = ReferenceLUFS - albumGain;
            }

            const float albumPeak
                = std::ranges::max_element(m_scannedTracks, [](const Track& track1, const Track& track2) {
                      return track1.rgTrackPeak() < track2.rgTrackPeak();
                  })->rgTrackPeak();

            for(Track& track : m_scannedTracks) {
                track.setRGAlbumGain(static_cast<float>(albumGain));
                track.setRGAlbumPeak(albumPeak);
            }
        }

        if(mayRun()) {
            emit calculationFinished(m_scannedTracks);
        }
        if(m_runningWatchers.fetch_sub(1, std::memory_order_release) <= 1) {
            emit finished();
        }
        setState(Idle);
    });
}

void Ebur128Scanner::calculateByAlbumTags(const TrackList& tracks, const QString& groupScript, bool truePeak)
{
    setState(Running);

    for(const auto& track : tracks) {
        const QString album = m_parser.evaluate(groupScript, track);
        m_albums[album].push_back(track);
    }

    m_currentAlbum = m_albums.begin();
    scanAlbum(truePeak);
}

void Ebur128Scanner::scanTrack(Track& track, bool truePeak, const QString& album)
{
    if(!mayRun()) {
        m_audioLoader->destroyThreadInstance();
        return;
    }

    auto* decoder = m_audioLoader->decoderForTrack(track);
    if(!decoder) {
        m_audioLoader->destroyThreadInstance();
        return;
    }

    AudioSource source;
    source.filepath = track.filepath();
    QFile file{source.filepath};
    if(!file.open(QIODevice::ReadOnly)) {
        qCWarning(EBUR128) << "Failed to open" << source.filepath;
        m_audioLoader->destroyThreadInstance();
        return;
    }
    source.device = &file;

    auto format = decoder->init(source, track, AudioDecoder::NoSeeking | AudioDecoder::NoInfiniteLooping);
    if(!format) {
        m_audioLoader->destroyThreadInstance();
        return;
    }

    format->setSampleFormat(SampleFormat::F64);
    decoder->start();

    EburStatePtr state{ebur128_init(format->channelCount(), format->sampleRate(),
                                    EBUR128_MODE_I | (truePeak ? EBUR128_MODE_TRUE_PEAK : EBUR128_MODE_SAMPLE_PEAK))};

    AudioBuffer buffer;
    while((buffer = decoder->readBuffer(BufferSize)).isValid()) {
        if(!mayRun()) {
            m_audioLoader->destroyThreadInstance();
            return;
        }

        buffer = Audio::convert(buffer, *format);
        if(ebur128_add_frames_double(state.get(), std::bit_cast<double*>(buffer.data()), buffer.frameCount())
           != EBUR128_SUCCESS) {
            break;
        }
    }

    if(!mayRun()) {
        m_audioLoader->destroyThreadInstance();
        return;
    }

    double trackGain{Constants::InvalidGain};
    if(ebur128_loudness_global(state.get(), &trackGain) == EBUR128_SUCCESS) {
        trackGain = ReferenceLUFS - trackGain;
        track.setRGTrackGain(static_cast<float>(trackGain));
    }

    double trackPeak{Constants::InvalidPeak};
    if((truePeak ? ebur128_true_peak(state.get(), 0, &trackPeak) : ebur128_sample_peak(state.get(), 0, &trackPeak))
       == EBUR128_SUCCESS) {
        trackPeak = std::pow(10, trackPeak / 20.0);
        track.setRGTrackPeak(static_cast<float>(trackPeak));
    }

    if(!album.isEmpty()) {
        const std::scoped_lock lock{m_mutex};
        m_albumStates[album].emplace_back(std::move(state));
    }

    m_audioLoader->destroyThreadInstance();
}

void Ebur128Scanner::scanAlbum(bool truePeak)
{
    if(m_currentAlbum == m_albums.cend()) {
        if(mayRun()) {
            for(const auto& [_, tracks] : m_albums) {
                m_scannedTracks.insert(m_scannedTracks.end(), tracks.cbegin(), tracks.cend());
            }
            emit calculationFinished(m_scannedTracks);
        }
        if(m_runningWatchers.fetch_sub(1, std::memory_order_release) <= 1) {
            emit finished();
        }
        setState(Idle);
        return;
    }

    m_tracks = m_currentAlbum->second;

    auto albumFuture = QtConcurrent::map(
        m_currentAlbum->second, [this, truePeak](Track& track) { scanTrack(track, truePeak, m_currentAlbum->first); });

    auto* albumWatcher = new QFutureWatcher<void>(this);
    m_albumWatchers.emplace(m_currentAlbum->first, albumWatcher);

    QObject::connect(albumWatcher, &QFutureWatcher<void>::progressValueChanged, this, [this](const int val) {
        if(val >= 0 && std::cmp_less(val, m_tracks.size())) {
            emit startingCalculation(m_tracks.at(val).prettyFilepath());
        }
    });

    QObject::connect(albumWatcher, &QFutureWatcher<void>::finished, this, [this, truePeak]() {
        const auto albumState = m_albumStates.find(m_currentAlbum->first);
        if(albumState != m_albumStates.cend()) {
            const auto& trackStates = albumState->second;
            std::vector<ebur128_state*> states;
            std::ranges::transform(trackStates, std::back_inserter(states),
                                   [](const auto& state) { return state.get(); });

            double albumGain{Constants::InvalidGain};
            if(ebur128_loudness_global_multiple(states.data(), states.size(), &albumGain) == EBUR128_SUCCESS) {
                albumGain = ReferenceLUFS - albumGain;
            }

            const float albumPeak
                = std::ranges::max_element(m_currentAlbum->second, [](const Track& track1, const Track& track2) {
                      return track1.rgTrackPeak() < track2.rgTrackPeak();
                  })->rgTrackPeak();

            for(Track& track : m_currentAlbum->second) {
                track.setRGAlbumGain(static_cast<float>(albumGain));
                track.setRGAlbumPeak(albumPeak);
            }
        }

        ++m_currentAlbum;
        scanAlbum(truePeak);
    });

    albumWatcher->setFuture(albumFuture);
    m_runningWatchers.fetch_add(1, std::memory_order_acquire);
}
} // namespace Fooyin::RGScanner
