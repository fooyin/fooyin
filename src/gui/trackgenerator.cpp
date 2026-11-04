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

#include "trackgenerator.h"

#include <QDateTime>
#include <QDir>
#include <QPainter>
#include <QStringList>
#include <QTimeZone>

#include <array>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
struct SampleFormat
{
    QString codec;
    QString extension;
    QString encoding;
    QString tagType;
    int bitrate{0};
    int sampleRate{0};
    int bitDepth{0};
};

struct SampleAlbum
{
    QString title;
    QString artist;
    QString guestArtist;
    QStringList composers;
    QStringList genres;
    QString date;
    QString label;
    QString catalogueNumber;
    std::array<QString, 6> titles;
    SampleFormat format;
};

const std::array<SampleAlbum, 3>& sampleAlbums()
{
    static const std::array albums{
        SampleAlbum{
            .title           = u"City After Midnight"_s,
            .artist          = u"The Static Hour"_s,
            .guestArtist     = u"Mina Vale"_s,
            .composers       = {u"Ada Mercer"_s, u"Jon Keene"_s},
            .genres          = {u"Synthpop"_s, u"Indie Pop"_s},
            .date            = u"2023-10-06"_s,
            .label           = u"Northline Records"_s,
            .catalogueNumber = u"NLR-042"_s,
            .titles = {u"Signal Fires"_s, u"The Last Train Across the City"_s, u"Distant Lights"_s, u"Glass Horizons"_s,
                       u"Conversations Beneath the Streetlights"_s, u"Home Before Dawn"_s,},
            .format = {.codec      = u"FLAC"_s,
                       .extension  = u"flac"_s,
                       .encoding   = u"Lossless"_s,
                       .tagType    = u"XiphComment"_s,
                       .bitrate    = 1050,
                       .sampleRate = 44100,
                       .bitDepth   = 24,},
        },
        SampleAlbum{
            .title           = u"Small Hours, Wide Streets"_s,
            .artist          = u"Ada Mercer Trio"_s,
            .guestArtist     = u"Émile Laurent"_s,
            .composers       = {u"Ada Mercer"_s},
            .genres          = {u"Jazz"_s, u"Contemporary Jazz"_s},
            .date            = u"2021-05-14"_s,
            .label           = u"Blue Lantern"_s,
            .catalogueNumber = u"BL-017"_s,
            .titles = {u"First Light"_s, u"Waltz for an Empty Room"_s, u"Café on the Corner"_s, u"Between Stations"_s,
                       u"A Quiet Conversation (Live)"_s, u"The Long Way Home"_s,},
            .format = {.codec      = u"MP3"_s,
                       .extension  = u"mp3"_s,
                       .encoding   = u"Lossy"_s,
                       .tagType    = u"ID3v2.4"_s,
                       .bitrate    = 320,
                       .sampleRate = 44100,
                       .bitDepth   = 0,},
        },
        SampleAlbum{
            .title           = u"Landscapes for Strings & Piano"_s,
            .artist          = u"Northbridge Chamber Orchestra"_s,
            .guestArtist     = u"Clara Weiss"_s,
            .composers       = {u"Sofia Lind"_s, u"Thomas Reed"_s},
            .genres          = {u"Classical"_s, u"Chamber Music"_s},
            .date            = u"2024-02-23"_s,
            .label           = u"Fieldwork Classics"_s,
            .catalogueNumber = u"FWC-108"_s,
            .titles          = {u"Northern Suite: I. Across the Water"_s, u"Northern Suite: II. Stillness"_s,
                                u"Northern Suite: III. The Returning Tide"_s, u"Three Miniatures: I. Dawn"_s,
                                u"Three Miniatures: II. Afternoon Rain"_s, u"Three Miniatures: III. Evening Song"_s,},
            .format          = {.codec      = u"FLAC"_s,
                                .extension  = u"flac"_s,
                                .encoding   = u"Lossless"_s,
                                .tagType    = u"XiphComment"_s,
                                .bitrate    = 2800,
                                .sampleRate = 96000,
                                .bitDepth   = 24,},
        },};
    return albums;
}

constexpr std::array<uint64_t, 6> Durations{222000, 318000, 547000, 264000, 391000, 286000};

Track createTrack(const TrackGenerator::Options& options, int albumIndex, int disc, int number)
{
    const QDir dir{options.basePath.isEmpty() ? u"/preview"_s : options.basePath};
    const auto& albums    = sampleAlbums();
    const auto albumCount = static_cast<int>(albums.size());
    const auto& album     = albums[albumIndex % albumCount];

    QString albumTitle{album.title};
    if(albumIndex >= albumCount) {
        albumTitle += u" (%1)"_s.arg((albumIndex / albumCount) + 1);
    }

    const auto addedTime = static_cast<uint64_t>(
        QDate::fromString(album.date, Qt::ISODate).addDays(4).startOfDay(QTimeZone{0}).toMSecsSinceEpoch());
    const float albumGain = -7.5F - static_cast<float>(albumIndex % albumCount);

    const auto pos   = ((disc - 1) * options.tracksPerDisc) + number - 1;
    const auto index = (albumIndex * options.discsPerAlbum * options.tracksPerDisc) + pos;

    QString title = album.titles[pos % album.titles.size()];
    if(std::cmp_greater_equal(pos, album.titles.size())) {
        title += u" (%1)"_s.arg((pos / album.titles.size()) + 1);
    }

    QString filename = u"%1 %2.%3"_s.arg(number, 2, 10, QLatin1Char{'0'}).arg(title, album.format.extension);
    filename.replace(':'_L1, '-'_L1);

    const QString path = u"%1/%2/Disc %3/%4"_s.arg(album.artist, albumTitle).arg(disc).arg(filename);
    const uint64_t duration
        = Durations[pos % Durations.size()] + (static_cast<size_t>(albumIndex % albumCount) * 47000);
    const int playCount = index % 5 == 4 ? 0 : 10 + ((index * 7) % 50);

    Track track{dir.filePath(path), 0};
    track.setTitle(title);

    QStringList artists{album.artist};
    if(pos % 3 == 0) {
        artists.push_back(album.guestArtist);
    }

    track.setArtists(artists);
    track.setAlbumArtists({album.artist});
    track.setAlbum(albumTitle);
    track.setTrackNumber(QString::number(number));
    track.setTrackTotal(QString::number(options.tracksPerDisc));
    track.setDiscNumber(QString::number(disc));
    track.setDiscTotal(QString::number(options.discsPerAlbum));
    track.setDate(album.date);
    track.setGenres(album.genres);
    track.setComposers(album.composers);
    track.setPerformers({album.artist, album.guestArtist});
    track.setComment(pos % 2 == 0 ? u"Album version"_s : u"Recorded live at the Northbridge Hall"_s);
    track.addExtraTag(u"LABEL"_s, album.label);
    track.addExtraTag(u"CATALOGNUMBER"_s, album.catalogueNumber);
    track.addExtraTag(u"DISC_SUBTITLE"_s, u"Volume %1"_s.arg(disc));
    track.addExtraTag(u"COUNTRY"_s, u"United Kingdom"_s);
    track.addExtraTag(u"MEDIA"_s, u"Digital"_s);
    track.setDuration(duration);
    track.setCodec(album.format.codec);
    track.setEncoding(album.format.encoding);
    track.setTagTypes({album.format.tagType});
    track.setBitrate(album.format.bitrate);
    track.setSampleRate(album.format.sampleRate);
    track.setSampleFrames((duration * album.format.sampleRate) / 1000);
    track.setBitDepth(album.format.bitDepth);
    track.setChannels(2);
    track.setFileSize((duration * album.format.bitrate) / 8);
    track.setRatingStars(10 - (index % 11));
    track.setPlayCount(playCount);
    track.setLoved(index % 4 == 0);
    track.setRGAlbumGain(albumGain);
    track.setRGTrackGain(albumGain + (static_cast<float>(pos % 5) * 0.5F));
    track.setRGAlbumPeak(0.99F);
    track.setRGTrackPeak(0.82F + (static_cast<float>(pos % 8) * 0.02F));
    track.setCreatedTime(addedTime - 86400000);
    track.setAddedTime(addedTime);
    track.setModifiedTime(addedTime);

    if(playCount > 0) {
        track.setFirstPlayed(addedTime + 3600000);
        track.setLastPlayed(addedTime + (static_cast<uint64_t>(playCount) * 86400000));
    }

    return track;
}
} // namespace

Track TrackGenerator::generateTrack(const QString& basePath)
{
    return createTrack({.basePath = basePath}, 0, 1, 1);
}

TrackList TrackGenerator::generateTracks()
{
    return generateTracks(Options{});
}

TrackList TrackGenerator::generateTracks(const Options& options)
{
    if(options.albumCount <= 0 || options.discsPerAlbum <= 0 || options.tracksPerDisc <= 0) {
        return {};
    }

    const int tracksPerAlbum = options.discsPerAlbum * options.tracksPerDisc;

    TrackList tracks;
    tracks.reserve(static_cast<size_t>(options.albumCount) * tracksPerAlbum);

    for(int albumIndex{0}; albumIndex < options.albumCount; ++albumIndex) {
        for(int disc{1}; disc <= options.discsPerAlbum; ++disc) {
            for(int number{1}; number <= options.tracksPerDisc; ++number) {
                tracks.push_back(createTrack(options, albumIndex, disc, number));
            }
        }
    }

    return tracks;
}

QPixmap TrackGenerator::generateArtwork()
{
    QPixmap artwork{512, 512};

    QPainter painter{&artwork};
    painter.setRenderHint(QPainter::Antialiasing);

    QLinearGradient sky{0, 0, 0, 512};
    sky.setColorAt(0.0, QColor{24, 32, 66});
    sky.setColorAt(1.0, QColor{179, 101, 100});
    painter.fillRect(artwork.rect(), sky);

    const QRect sun{320, 88, 96, 96};
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor{242, 199, 145});
    painter.drawEllipse(sun);

    static constexpr std::array Heights{140, 190, 120, 240, 175, 210, 130, 260, 180, 150, 220, 165};
    int x{0};
    for(const int height : Heights) {
        painter.fillRect(x, 512 - height, 44, height, QColor{26, 33, 53});
        for(int y{528 - height}; y < 490; y += 24) {
            painter.fillRect(x + 12, y, 5, 8, QColor{222, 170, 115});
            painter.fillRect(x + 28, y, 5, 8, QColor{222, 170, 115});
        }
        x += 44;
    }

    return artwork;
}
} // namespace Fooyin
