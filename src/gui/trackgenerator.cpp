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
#include <QPainterPath>
#include <QRandomGenerator>
#include <QStringList>
#include <QTimeZone>

#include <array>
#include <cmath>
#include <numbers>

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

QString musicTitle(size_t index)
{
    static const std::array Adjectives{
        u"Amber"_s,  u"Autumn"_s, u"Black"_s,    u"Blue"_s,   u"Broken"_s, u"Burning"_s, u"Cedar"_s,  u"Copper"_s,
        u"Empty"_s,  u"Fading"_s, u"Golden"_s,   u"Hidden"_s, u"Hollow"_s, u"Indigo"_s,  u"Lunar"_s,  u"Midnight"_s,
        u"Modern"_s, u"Neon"_s,   u"Northern"_s, u"Pale"_s,   u"Paper"_s,  u"Quiet"_s,   u"Silver"_s, u"Slow"_s,
        u"Soft"_s,   u"Static"_s, u"Velvet"_s,   u"Violet"_s, u"White"_s,  u"Wild"_s,    u"Winter"_s, u"Wooden"_s,
    };
    static const std::array Nouns{
        u"Anchors"_s, u"Arches"_s,  u"Birds"_s,   u"Cities"_s, u"Clouds"_s,  u"Coast"_s,   u"Echoes"_s,  u"Fields"_s,
        u"Fires"_s,   u"Flowers"_s, u"Gardens"_s, u"Ghosts"_s, u"Harbour"_s, u"Hearts"_s,  u"Hills"_s,   u"Islands"_s,
        u"Letters"_s, u"Maps"_s,    u"Mirrors"_s, u"Oceans"_s, u"Orbits"_s,  u"Pines"_s,   u"Rivers"_s,  u"Roads"_s,
        u"Rooms"_s,   u"Shadows"_s, u"Skies"_s,   u"Stars"_s,  u"Tides"_s,   u"Valleys"_s, u"Windows"_s, u"Wires"_s,
    };
    static constexpr auto combinations = Adjectives.size() * Nouns.size();

    const auto pair = ((index % combinations) * 37) % combinations;
    QString title   = u"%1 %2"_s.arg(Adjectives[pair % Adjectives.size()], Nouns[pair / Adjectives.size()]);

    const auto block = index / combinations;
    if(block > 0) {
        title = u"%1 of %2"_s.arg(Nouns[(block - 1) % Nouns.size()], title);
        if(block > Nouns.size()) {
            title += u" (%1)"_s.arg(((block - 1) / Nouns.size()) + 1);
        }
    }

    return title;
}

QString artistName(size_t index)
{
    static const std::array FirstNames{
        u"Alice"_s, u"André"_s, u"Clara"_s, u"Elena"_s, u"Félix"_s,  u"Hana"_s,  u"José"_s, u"Kai"_s, u"Léa"_s,
        u"Luca"_s,  u"Maya"_s,  u"Mei"_s,   u"Jing"_s,  u"Rafael"_s, u"Sofia"_s, u"Zoë"_s,  u"Wei"_s,
    };
    static const std::array LastNames{
        u"Brooks"_s, u"Chen"_s,   u"Costa"_s, u"Dubois"_s, u"Fischer"_s, u"García"_s, u"Ito"_s,   u"Kim"_s,
        u"Lee"_s,    u"Müller"_s, u"Novak"_s, u"Reed"_s,   u"Silva"_s,   u"Walsh"_s,  u"Zhang"_s, u"Liu"_s,
    };

    const auto nameIndex = index / 2;
    if(index % 2 != 0) {
        return u"The %1 Collective"_s.arg(musicTitle(nameIndex));
    }

    QString name = u"%1 %2"_s.arg(FirstNames[nameIndex % FirstNames.size()],
                                  LastNames[(nameIndex / FirstNames.size()) % LastNames.size()]);

    const auto block = nameIndex / (FirstNames.size() * LastNames.size());
    if(block > 0) {
        name += u" (%1)"_s.arg(block + 1);
    }

    return name;
}

SampleAlbum createAlbum(int albumIndex)
{
    const auto& albums = sampleAlbums();
    auto album         = albums[albumIndex % albums.size()];
    if(std::cmp_less(albumIndex, albums.size())) {
        return album;
    }

    const auto index = static_cast<size_t>(albumIndex);

    album.title       = musicTitle(index);
    album.artist      = artistName(index);
    album.guestArtist = artistName(index + 31);
    album.composers   = {artistName(index * 2), artistName((index * 2) + 14)};
    album.date
        = QDate{1970 + (albumIndex % 55), 1 + ((albumIndex * 7) % 12), 1 + (albumIndex % 28)}.toString(Qt::ISODate);
    album.label           = u"%1 Records"_s.arg(musicTitle(128 + (index / 3)));
    album.catalogueNumber = u"%1-%2"_s.arg(album.label.left(2).toUpper()).arg(albumIndex + 1, 4, 10, '0'_L1);

    return album;
}

Track createTrack(const TrackGenerator::Options& options, const SampleAlbum& album, int albumIndex, int disc,
                  int number)
{
    const QDir dir{options.basePath.isEmpty() ? u"/preview"_s : options.basePath};

    const auto addedTime = static_cast<uint64_t>(
        QDate::fromString(album.date, Qt::ISODate).addDays(4).startOfDay(QTimeZone{0}).toMSecsSinceEpoch());
    const float albumGain = -7.5F - static_cast<float>(albumIndex % sampleAlbums().size());

    const auto pos   = ((disc - 1) * options.tracksPerDisc) + number - 1;
    const auto index = (albumIndex * options.discsPerAlbum * options.tracksPerDisc) + pos;

    const QString title = std::cmp_less(albumIndex, sampleAlbums().size()) && std::cmp_less(pos, album.titles.size())
                            ? album.titles[pos]
                            : musicTitle(static_cast<size_t>(index));

    QString filename = u"%1 %2.%3"_s.arg(number, 2, 10, QLatin1Char{'0'}).arg(title, album.format.extension);
    filename.replace(':'_L1, '-'_L1);

    const QString path      = u"%1/%2/Disc %3/%4"_s.arg(album.artist, album.title).arg(disc).arg(filename);
    const uint64_t duration = Durations[pos % Durations.size()] + ((static_cast<uint64_t>(albumIndex) * 7919) % 47000);
    const int playCount     = index % 5 == 4 ? 0 : 10 + ((index * 7) % 50);

    Track track{dir.filePath(path), 0};
    track.setTitle(title);

    QStringList artists{album.artist};
    if(pos % 3 == 0) {
        artists.push_back(album.guestArtist);
    }

    track.setArtists(artists);
    track.setAlbumArtists({album.artist});
    track.setAlbum(album.title);
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

struct ArtworkPalette
{
    QColor background;
    QColor horizon;
    QColor foreground;
    QColor accent;
    QColor highlight;
};

void paintSkyline(QPainter& painter, QRandomGenerator& random, const ArtworkPalette& palette)
{
    painter.setBrush(palette.highlight);
    const int radius = random.bounded(32, 64);
    painter.drawEllipse(QPoint{random.bounded(80, 432), random.bounded(80, 180)}, radius, radius);

    for(int layer{0}; layer < 2; ++layer) {
        const int width     = layer == 0 ? 36 : 48;
        const QColor colour = layer == 0 ? palette.foreground.lighter(150) : palette.foreground;

        for(int x{-12}; x < 512; x += width) {
            const int height = random.bounded(70, 240) + (layer * 40);
            painter.fillRect(x, 512 - height, width - 3, height, colour);

            if(layer == 0) {
                continue;
            }

            for(int y{528 - height}; y < 500; y += 22) {
                for(int windowX{x + 10}; windowX < x + width - 8; windowX += 14) {
                    if(random.bounded(3) != 0) {
                        painter.fillRect(windowX, y, 4, 7, palette.highlight);
                    }
                }
            }
        }
    }
}

void paintMountains(QPainter& painter, QRandomGenerator& random, const ArtworkPalette& palette)
{
    painter.setBrush(palette.highlight);
    const int radius = random.bounded(40, 80);
    painter.drawEllipse(QPoint{random.bounded(100, 412), random.bounded(100, 200)}, radius, radius);

    const std::array colours{palette.horizon, palette.accent, palette.foreground.lighter(150), palette.foreground};
    for(int layer{0}; layer < 4; ++layer) {
        const int baseline = 300 + (layer * 65);

        QPainterPath ridge;
        ridge.moveTo(-64, 512);
        for(int x{-64}; x <= 576; x += 64) {
            ridge.lineTo(x, baseline - random.bounded(20, 140));
        }
        ridge.lineTo(576, 512);
        ridge.closeSubpath();

        painter.fillPath(ridge, colours[layer]);
    }
}

void paintOrbits(QPainter& painter, QRandomGenerator& random, const ArtworkPalette& palette)
{
    const QPointF centre{static_cast<qreal>(random.bounded(210, 302)), static_cast<qreal>(random.bounded(210, 302))};
    painter.setBrush(palette.foreground);
    painter.drawEllipse(centre, 196, 196);
    painter.setBrush(Qt::NoBrush);

    for(int ring{0}; ring < 7; ++ring) {
        const qreal radius = 50 + (ring * 24);
        painter.setPen(
            QPen{ring % 2 == 0 ? palette.accent : palette.horizon, static_cast<qreal>(random.bounded(2, 7))});
        painter.drawEllipse(centre, radius, radius);

        const qreal angle       = random.generateDouble() * 2.0 * std::numbers::pi;
        const QPointF satellite = centre + QPointF{std::cos(angle) * radius, std::sin(angle) * radius};
        const int size          = random.bounded(6, 16);

        painter.setPen(Qt::NoPen);
        painter.setBrush(palette.highlight);
        painter.drawEllipse(satellite, size, size);
        painter.setBrush(Qt::NoBrush);
    }

    painter.setBrush(palette.accent);
    painter.drawEllipse(centre, 26, 26);
}

void paintGeometric(QPainter& painter, QRandomGenerator& random, const ArtworkPalette& palette)
{
    painter.fillRect(QRect{0, 0, 512, 512}, palette.background);

    const std::array colours{palette.horizon, palette.foreground, palette.accent, palette.highlight};

    for(int row{0}; row < 4; ++row) {
        for(int column{0}; column < 4; ++column) {
            painter.save();
            painter.translate(88 + (column * 112), 88 + (row * 112));
            painter.rotate(random.bounded(4) * 90);

            const QRect tile{-56, -56, 112, 112};
            painter.setClipRect(tile);
            painter.setBrush(colours[random.bounded(4)]);

            switch(random.bounded(4)) {
                case 0:
                    painter.drawRect(tile);
                    break;
                case 1:
                    painter.drawEllipse(tile);
                    break;
                case 2: {
                    QPainterPath triangle;
                    triangle.moveTo(tile.topLeft());
                    triangle.lineTo(tile.topRight());
                    triangle.lineTo(tile.bottomLeft());
                    triangle.closeSubpath();
                    painter.drawPath(triangle);
                    break;
                }
                case 3:
                    painter.drawEllipse(tile.topLeft(), 112, 112);
                    break;
                default:;
            }

            painter.restore();
        }
    }
}

void paintWaves(QPainter& painter, QRandomGenerator& random, const ArtworkPalette& palette)
{
    const qreal phase     = random.generateDouble() * 2.0 * std::numbers::pi;
    const qreal frequency = 0.008 + (random.generateDouble() * 0.008);
    const qreal amplitude = random.bounded(35, 85);
    const std::array colours{palette.foreground, palette.accent, palette.highlight};

    painter.setBrush(Qt::NoBrush);

    for(int line{0}; line < 18; ++line) {
        QPainterPath wave;
        for(int x{-32}; x <= 544; x += 8) {
            const qreal y = (line * 36) - 50 + (std::sin((x * frequency) + phase + (line * 0.18)) * amplitude);
            if(x == -32) {
                wave.moveTo(x, y);
            }
            else {
                wave.lineTo(x, y);
            }
        }
        painter.setPen(QPen{colours[line % colours.size()], static_cast<qreal>(random.bounded(8, 18)), Qt::SolidLine,
                            Qt::RoundCap});
        painter.drawPath(wave);
    }
}
} // namespace

Track TrackGenerator::generateTrack(const QString& basePath)
{
    return createTrack({.basePath = basePath}, createAlbum(0), 0, 1, 1);
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
        const auto album = createAlbum(albumIndex);
        for(int disc{1}; disc <= options.discsPerAlbum; ++disc) {
            for(int number{1}; number <= options.tracksPerDisc; ++number) {
                tracks.push_back(createTrack(options, album, albumIndex, disc, number));
            }
        }
    }

    return tracks;
}

QPixmap TrackGenerator::generateArtwork()
{
    return generateArtwork(0);
}

QPixmap TrackGenerator::generateArtwork(uint32_t seed)
{
    static const std::array palettes{
        ArtworkPalette{
            .background = qRgb(24, 32, 66),
            .horizon    = qRgb(179, 101, 100),
            .foreground = qRgb(26, 33, 53),
            .accent     = qRgb(225, 124, 100),
            .highlight  = qRgb(242, 199, 145),
        },
        ArtworkPalette{.background = qRgb(15, 53, 55),
                       .horizon    = qRgb(69, 125, 113),
                       .foreground = qRgb(10, 37, 40),
                       .accent     = qRgb(227, 154, 66),
                       .highlight  = qRgb(241, 222, 170)},
        ArtworkPalette{.background = qRgb(247, 233, 204),
                       .horizon    = qRgb(222, 181, 139),
                       .foreground = qRgb(67, 76, 91),
                       .accent     = qRgb(201, 88, 59),
                       .highlight  = qRgb(255, 248, 228)},
        ArtworkPalette{.background = qRgb(49, 25, 76),
                       .horizon    = qRgb(133, 86, 151),
                       .foreground = qRgb(36, 20, 64),
                       .accent     = qRgb(236, 134, 171),
                       .highlight  = qRgb(246, 215, 192)},
        ArtworkPalette{.background = qRgb(22, 58, 87),
                       .horizon    = qRgb(84, 142, 170),
                       .foreground = qRgb(18, 36, 66),
                       .accent     = qRgb(86, 205, 199),
                       .highlight  = qRgb(221, 244, 230)},
        ArtworkPalette{.background = qRgb(83, 32, 43),
                       .horizon    = qRgb(182, 87, 87),
                       .foreground = qRgb(48, 25, 39),
                       .accent     = qRgb(236, 156, 95),
                       .highlight  = qRgb(250, 222, 185)},
        ArtworkPalette{.background = qRgb(221, 230, 224),
                       .horizon    = qRgb(139, 169, 158),
                       .foreground = qRgb(35, 68, 67),
                       .accent     = qRgb(194, 110, 79),
                       .highlight  = qRgb(248, 239, 208)},
        ArtworkPalette{.background = qRgb(27, 30, 44),
                       .horizon    = qRgb(80, 85, 123),
                       .foreground = qRgb(14, 17, 30),
                       .accent     = qRgb(156, 157, 222),
                       .highlight  = qRgb(233, 211, 168)},
    };

    const auto& palette = palettes[seed % palettes.size()];

    QRandomGenerator random{seed};
    QPixmap artwork{512, 512};

    QPainter painter{&artwork};
    painter.setRenderHint(QPainter::Antialiasing);

    QLinearGradient background{0, 0, 512, 512};
    background.setColorAt(0.0, palette.background);
    background.setColorAt(1.0, palette.horizon);
    painter.fillRect(artwork.rect(), background);
    painter.setPen(Qt::NoPen);

    switch(seed % 5) {
        case 0:
            paintSkyline(painter, random, palette);
            break;
        case 1:
            paintMountains(painter, random, palette);
            break;
        case 2:
            paintOrbits(painter, random, palette);
            break;
        case 3:
            paintGeometric(painter, random, palette);
            break;
        case 4:
            paintWaves(painter, random, palette);
            break;
        default:;
    }

    return artwork;
}
} // namespace Fooyin
