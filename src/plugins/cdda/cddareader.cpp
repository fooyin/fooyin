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
 */

#include "cddareader.h"

#include "cddatoc.h"
#include "cddaurl.h"

using namespace Qt::StringLiterals;

constexpr auto SampleRate = 44100;
constexpr auto Channels   = 2;
constexpr auto BitDepth   = 16;
constexpr auto Bitrate    = 1411;

namespace Fooyin::Cdda {
namespace {

std::expected<void, QString> populateTrack(Track& track, const CdTocTrack& tocTrack, int trackTotal,
                                           const QString& discId, const CdText& cdText)
{
    const auto duration = durationForSectors(static_cast<uint64_t>(tocTrack.endSectorExclusive - tocTrack.firstSector));
    if(!duration) {
        return std::unexpected(duration.error());
    }

    const QString physicalTrackNumber = QString::number(tocTrack.number);
    CdTextFields trackText;
    if(cdText.tracks.contains(tocTrack.number)) {
        trackText = cdText.tracks.at(tocTrack.number);
    }

    if(track.title().isEmpty()) {
        track.setTitle(trackText.title.isEmpty() ? CddaReader::tr("Track %1").arg(tocTrack.number, 2, 10, QChar{u'0'})
                                                 : trackText.title);
    }
    if(track.album().isEmpty()) {
        track.setAlbum(cdText.disc.title.isEmpty() ? CddaReader::tr("Audio CD") : cdText.disc.title);
    }
    if(track.artists().isEmpty()) {
        const QString artist = trackText.performer.isEmpty() ? cdText.disc.performer : trackText.performer;
        if(!artist.isEmpty()) {
            track.setArtists({artist});
        }
    }
    if(track.albumArtists().isEmpty() && !cdText.disc.performer.isEmpty()) {
        track.setAlbumArtists({cdText.disc.performer});
    }
    if(track.genres().isEmpty()) {
        const QString genre = trackText.genre.isEmpty() ? cdText.disc.genre : trackText.genre;
        if(!genre.isEmpty()) {
            track.setGenres({genre});
        }
    }
    if(track.composers().isEmpty()) {
        const QString composer = trackText.composer.isEmpty() ? cdText.disc.composer : trackText.composer;
        if(!composer.isEmpty()) {
            track.setComposers({composer});
        }
    }
    if(track.comment().isEmpty()) {
        track.setComment(trackText.message.isEmpty() ? cdText.disc.message : trackText.message);
    }
    if(!track.hasExtraTag(u"ISRC"_s) && !trackText.isrc.isEmpty()) {
        track.addExtraTag(u"ISRC"_s, trackText.isrc);
    }
    if(track.trackNumber().isEmpty()) {
        track.setTrackNumber(physicalTrackNumber);
    }
    if(track.trackTotal().isEmpty()) {
        track.setTrackTotal(QString::number(trackTotal));
    }

    track.setDuration(*duration);
    track.setSampleRate(SampleRate);
    track.setChannels(Channels);
    track.setBitDepth(BitDepth);
    track.setBitrate(Bitrate);
    track.setCodec(u"CDDA"_s);
    track.setEncoding(u"Lossless"_s);
    track.setExtraProperty(u"_CDDA_DISC_ID"_s, discId);
    track.setExtraProperty(u"_CDDA_TRACK_NUMBER"_s, physicalTrackNumber);

    return {};
}
} // namespace

std::expected<TrackList, QString> tracksForDisc(const CdToc& toc, const QString& discId, const CdText& cdText)
{
    if(const auto valid = validateToc(toc); !valid) {
        return std::unexpected(invalidTocUserMessage());
    }

    const QString filepath = cddaUrl(discId);
    if(filepath.isEmpty()) {
        return std::unexpected(CddaReader::tr("Invalid audio CD identity"));
    }

    const std::vector<CdTocTrack> tocTracks = audioTracks(toc);

    TrackList tracks;
    tracks.reserve(tocTracks.size());

    const auto trackTotal = static_cast<int>(tocTracks.size());
    for(int subsong{0}; subsong < trackTotal; ++subsong) {
        Track track{filepath, subsong};

        if(const auto result = populateTrack(track, tocTracks.at(subsong), trackTotal, discId, cdText); !result) {
            return std::unexpected(result.error());
        }

        track.setMetadataWasRead(true);
        tracks.push_back(std::move(track));
    }

    return tracks;
}

CddaReader::CddaReader(std::shared_ptr<CdDriveManager> driveManager)
    : m_driveManager{std::move(driveManager)}
{ }

QStringList CddaReader::extensions() const
{
    return {};
}

QStringList CddaReader::supportedSchemes() const
{
    return {QString::fromLatin1(Scheme)};
}

bool CddaReader::canReadCover() const
{
    return false;
}

bool CddaReader::canWriteMetaData() const
{
    return false;
}

int CddaReader::subsongCount() const
{
    return static_cast<int>(m_audioTracks.size());
}

bool CddaReader::init(const AudioSource& source)
{
    clear();

    const auto discId = discIdFromCddaUrl(source.filepath);
    if(!discId) {
        m_error = u"Invalid audio CD URI"_s;
        return false;
    }

    auto observation = m_driveManager->resolveDisc(*discId);
    if(!observation) {
        m_error = observation.error().message;
        return false;
    }
    if(!observation->toc) {
        m_error = u"Audio CD TPC is unavailable"_s;
        return false;
    }

    m_sourcePath  = source.filepath;
    m_discId      = *discId;
    m_toc         = *observation->toc;
    m_cdText      = observation->cdText.value_or(CdText{});
    m_audioTracks = audioTracks(m_toc);

    return !m_audioTracks.empty();
}

bool CddaReader::readTrack(const AudioSource& source, Track& track)
{
    m_error.clear();

    if(m_sourcePath.isEmpty() || source.filepath != m_sourcePath || track.filepath() != m_sourcePath) {
        m_error = u"Audio CD reader source changed after initialization"_s;
        return false;
    }

    const auto trackTotal = static_cast<int>(m_audioTracks.size());

    const int subsong = track.subsong();
    if(subsong < 0 || subsong >= trackTotal) {
        m_error = u"Audio CD track index is out of range"_s;
        return false;
    }

    const auto result = populateTrack(track, m_audioTracks.at(subsong), trackTotal, m_discId, m_cdText);
    if(!result) {
        m_error = result.error();
    }
    return result.has_value();
}

QString CddaReader::lastError() const
{
    return m_error;
}

void CddaReader::clear()
{
    m_sourcePath.clear();
    m_discId.clear();
    m_error.clear();
    m_toc    = {};
    m_cdText = {};
    m_audioTracks.clear();
}
} // namespace Fooyin::Cdda
