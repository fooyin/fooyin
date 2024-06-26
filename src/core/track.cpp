/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "core/constants.h"
#include <core/track.h>

#include <utils/crypto.h>
#include <utils/utils.h>

#include <QDir>
#include <QFileInfo>
#include <QIODevice>

constexpr auto MaxStarCount = 5; // TODO: Support 10/half stars

namespace Fooyin {
struct Track::Private : public QSharedData
{
    int libraryId{-1};
    bool enabled{true};
    int id{-1};
    QString hash;
    Type type{0};
    QString filepath;
    QString relativePath;
    QString directory;
    QString filename;
    QString extension;
    QString title;
    QStringList artists;
    QString album;
    QStringList albumArtists;
    int trackNumber{-1};
    int trackTotal{-1};
    int discNumber{-1};
    int discTotal{-1};
    QStringList genres;
    QString composer;
    QString performer;
    QString comment;
    QString date;
    int year{-1};
    ExtraTags extraTags;
    QStringList removedTags;

    QString cuePath;

    uint64_t offset{0};
    uint64_t duration{0};
    uint64_t filesize{0};
    int bitrate{0};
    int sampleRate{0};
    int channels{2};
    int bitDepth{-1};

    float rating{-1};
    int playcount{0};
    uint64_t addedTime{0};
    uint64_t modifiedTime{0};
    uint64_t firstPlayed{0};
    uint64_t lastPlayed{0};

    QString sort;

    bool metadataWasModified{false};
};

Track::Track()
    : Track{QStringLiteral("")}
{ }

Track::Track(const QString& filepath)
    : p{new Private()}
{
    setFilePath(filepath);
}

bool Track::operator==(const Track& other) const
{
    return filepath() == other.filepath() && offset() == other.offset() && hash() == other.hash();
}

bool Track::operator!=(const Track& other) const
{
    return !(*this == other);
}

Track::~Track()                             = default;
Track::Track(const Track& other)            = default;
Track& Track::operator=(const Track& other) = default;

QString Track::generateHash()
{
    QString title = p->title;
    if(title.isEmpty()) {
        title = p->directory + p->filename;
    }

    p->hash = Utils::generateHash(p->artists.join(QStringLiteral(",")), p->album, QString::number(p->discNumber),
                                  QString::number(p->trackNumber), title);
    return p->hash;
}

bool Track::isValid() const
{
    return !p->filepath.isEmpty();
}

bool Track::isEnabled() const
{
    return p->enabled;
}

bool Track::isInLibrary() const
{
    return p->libraryId >= 0;
}

bool Track::isInDatabase() const
{
    return p->id >= 0;
}

bool Track::metadataWasRead() const
{
    // Assume read if basic properties are valid
    return p->type != Type::Unknown && p->filesize > 0 && p->modifiedTime > 0;
}

bool Track::metadataWasModified() const
{
    return p->metadataWasModified;
}

int Track::libraryId() const
{
    return p->libraryId;
}

int Track::id() const
{
    return p->id;
}

QString Track::hash() const
{
    return p->hash;
}

QString Track::albumHash() const
{
    QStringList hash;

    if(!p->date.isEmpty()) {
        hash.append(p->date);
    }
    if(!p->albumArtists.isEmpty()) {
        hash.append(p->albumArtists.join(QStringLiteral(",")));
    }
    if(!p->artists.isEmpty()) {
        hash.append(p->artists.join(QStringLiteral(",")));
    }

    if(!p->album.isEmpty()) {
        hash.append(p->album);
    }
    else {
        hash.append(p->directory);
    }

    return hash.join(QStringLiteral("|"));
}

Track::Type Track::type() const
{
    return p->type;
}

QString Track::typeString() const
{
    switch(p->type) {
        case(Type::MPEG):
            return QStringLiteral("MP3");
        case(Type::AIFF):
            return QStringLiteral("AIFF");
        case(Type::WAV):
            return QStringLiteral("PCM");
        case(Type::MPC):
            return QStringLiteral("MPC");
        case(Type::APE):
            return QStringLiteral("APE");
        case(Type::WavPack):
            return QStringLiteral("WavPack");
        case(Type::MP4):
            return QStringLiteral("MP4");
        case(Type::FLAC):
            return QStringLiteral("FLAC");
        case(Type::OggOpus):
            return QStringLiteral("Opus");
        case(Type::OggVorbis):
            return QStringLiteral("Vorbis");
        case(Type::ASF):
            return QStringLiteral("ASF");
        case(Type::Unknown):
        default:
            return QStringLiteral("Unknown");
    }
}

QString Track::filepath() const
{
    return p->filepath;
}

QString Track::uniqueFilepath() const
{
    QString path{p->filepath};

    if(hasCue()) {
        path.append(QString::number(p->offset));
    }

    return path;
}

QString Track::relativePath() const
{
    return p->relativePath;
}

QString Track::filename() const
{
    return p->filename;
}

QString Track::path() const
{
    return QFileInfo{p->filepath}.absolutePath();
}

QString Track::extension() const
{
    return p->extension;
}

QString Track::filenameExt() const
{
    return QFileInfo{p->filepath}.fileName();
}

QString Track::title() const
{
    return p->title;
}

QStringList Track::artists() const
{
    return p->artists;
}

QStringList Track::uniqueArtists() const
{
    QStringList artists;
    for(const QString& artist : p->artists) {
        if(!p->albumArtists.contains(artist)) {
            artists.emplace_back(artist);
        }
    }
    return artists;
}

QString Track::artist() const
{
    return p->artists.empty() ? QStringLiteral("") : p->artists.join(u"\037");
}

QString Track::primaryArtist() const
{
    if(!artists().empty()) {
        return artist();
    }
    if(!albumArtists().empty()) {
        return albumArtist();
    }
    if(!composer().isEmpty()) {
        return composer();
    }
    return performer();
}

QString Track::uniqueArtist() const
{
    const auto uniqArtists = uniqueArtists();
    return uniqArtists.isEmpty() ? QStringLiteral("") : uniqArtists.join(u"\037");
}

QString Track::album() const
{
    return p->album;
}

QStringList Track::albumArtists() const
{
    return p->albumArtists;
}

QString Track::albumArtist() const
{
    return p->albumArtists.empty() ? QStringLiteral("") : p->albumArtists.join(u"\037");
}

QString Track::primaryAlbumArtist() const
{
    if(!albumArtists().empty()) {
        return albumArtist();
    }
    if(!artists().empty()) {
        return artist();
    }
    if(!composer().isEmpty()) {
        return composer();
    }
    return performer();
}

int Track::trackNumber() const
{
    return p->trackNumber;
}

int Track::trackTotal() const
{
    return p->trackTotal;
}

int Track::discNumber() const
{
    return p->discNumber;
}

int Track::discTotal() const
{
    return p->discTotal;
}

QStringList Track::genres() const
{
    return p->genres;
}

QString Track::genre() const
{
    return p->genres.empty() ? QStringLiteral("") : p->genres.join(u"\037");
}

QString Track::composer() const
{
    return p->composer;
}

QString Track::performer() const
{
    return p->performer;
}

QString Track::comment() const
{
    return p->comment;
}

QString Track::date() const
{
    return p->date;
}

int Track::year() const
{
    return p->year;
}

float Track::rating() const
{
    return p->rating;
}

int Track::ratingStars() const
{
    return static_cast<int>(std::floor(p->rating * MaxStarCount));
}

bool Track::hasCue() const
{
    return !p->cuePath.isEmpty();
}

QString Track::cuePath() const
{
    return p->cuePath;
}

bool Track::hasExtraTag(const QString& tag) const
{
    return p->extraTags.contains(tag);
}

QStringList Track::extraTag(const QString& tag) const
{
    if(p->extraTags.contains(tag)) {
        return p->extraTags.value(tag);
    }
    return {};
}

Track::ExtraTags Track::extraTags() const
{
    return p->extraTags;
}

QStringList Track::removedTags() const
{
    return p->removedTags;
}

QByteArray Track::serialiseExtrasTags() const
{
    if(p->extraTags.empty()) {
        return {};
    }

    QByteArray out;
    QDataStream stream(&out, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);

    stream << p->extraTags;

    return out;
}

uint64_t Track::offset() const
{
    return p->offset;
}

uint64_t Track::duration() const
{
    return p->duration;
}

uint64_t Track::fileSize() const
{
    return p->filesize;
}

int Track::bitrate() const
{
    return p->bitrate;
}

int Track::sampleRate() const
{
    return p->sampleRate;
}

int Track::channels() const
{
    return p->channels;
}

int Track::bitDepth() const
{
    return p->bitDepth;
}

int Track::playCount() const
{
    return p->playcount;
}

uint64_t Track::addedTime() const
{
    return p->addedTime;
}

uint64_t Track::modifiedTime() const
{
    return p->modifiedTime;
}

QString Track::lastModified() const
{
    return Utils::formatTimeMs(p->modifiedTime);
}

uint64_t Track::firstPlayed() const
{
    return p->firstPlayed;
}

uint64_t Track::lastPlayed() const
{
    return p->lastPlayed;
}

QString Track::sort() const
{
    return p->sort;
}

void Track::setLibraryId(int id)
{
    p->libraryId = id;
}

void Track::setIsEnabled(bool enabled)
{
    p->enabled = enabled;
}

void Track::setId(int id)
{
    p->id = id;
}

void Track::setHash(const QString& hash)
{
    p->hash = hash;
}

void Track::setType(Type type)
{
    p->type = type;
}

void Track::setFilePath(const QString& path)
{
    p->filepath = path;

    if(!path.isEmpty()) {
        const QFileInfo fileInfo{path};
        p->filename  = fileInfo.completeBaseName();
        p->extension = fileInfo.suffix();
        p->directory = fileInfo.dir().dirName();
    }
}

void Track::setRelativePath(const QString& path)
{
    p->relativePath = path;
}

void Track::setTitle(const QString& title)
{
    p->title = title;

    if(!p->hash.isEmpty()) {
        generateHash();
    }
}

void Track::setArtists(const QStringList& artists)
{
    if(artists.size() == 1 && artists.front().isEmpty()) {
        p->artists.clear();
    }
    else {
        p->artists = artists;
    }

    if(!p->hash.isEmpty()) {
        generateHash();
    }
}

void Track::setAlbum(const QString& title)
{
    p->album = title;

    if(!p->hash.isEmpty()) {
        generateHash();
    }
}

void Track::setAlbumArtists(const QStringList& artists)
{
    if(artists.size() == 1 && artists.front().isEmpty()) {
        p->albumArtists.clear();
    }
    else {
        p->albumArtists = artists;
    }
}

void Track::setTrackNumber(int number)
{
    p->trackNumber = number;

    if(!p->hash.isEmpty()) {
        generateHash();
    }
}

void Track::setTrackTotal(int total)
{
    p->trackTotal = total;
}

void Track::setDiscNumber(int number)
{
    p->discNumber = number;

    if(!p->hash.isEmpty()) {
        generateHash();
    }
}

void Track::setDiscTotal(int total)
{
    p->discTotal = total;
}

void Track::setGenres(const QStringList& genres)
{
    if(genres.size() == 1 && genres.front().isEmpty()) {
        p->genres.clear();
    }
    else {
        p->genres = genres;
    }
}

void Track::setComposer(const QString& composer)
{
    p->composer = composer;
}

void Track::setPerformer(const QString& performer)
{
    p->performer = performer;
}

void Track::setComment(const QString& comment)
{
    p->comment = comment;
}

void Track::setDate(const QString& date)
{
    p->date = date;

    const QStringList dateParts = date.split(u'-', Qt::SkipEmptyParts);
    if(dateParts.empty()) {
        if(date.length() == 4) {
            p->year = p->date.toInt();
        }
    }
    else {
        const auto& year = dateParts.front();
        if(year.length() == 4) {
            p->year = year.toInt();
        }
    }
}

void Track::setYear(int year)
{
    p->year = year;
}

void Track::setRating(float rating)
{
    if(rating > 0 && rating < 1.0) {
        p->rating = rating;
    }
    else {
        p->rating = -1;
    }
}

void Track::setRatingStars(int rating)
{
    if(rating == 0) {
        p->rating = -1;
    }
    else {
        p->rating = static_cast<float>(rating) / MaxStarCount;
    }
}

QString Track::metaValue(const QString& name) const
{
    auto validNum = [](auto num) -> QString {
        if(num > 0) {
            return QString::number(num);
        }
        return {};
    };

    // clang-format off
    static const std::unordered_map<QString, std::function<QString(const Track& track)>> metaMap{
        {QLatin1String(Constants::MetaData::Title),       [](const Track& track) { return track.p->title; }},
        {QLatin1String(Constants::MetaData::Artist),      [](const Track& track) { return track.p->artists.join(QStringLiteral("\037")); }},
        {QLatin1String(Constants::MetaData::Album),       [](const Track& track) { return track.p->album; }},
        {QLatin1String(Constants::MetaData::AlbumArtist), [](const Track& track) { return track.p->albumArtists.join(QStringLiteral("\037")); }},
        {QLatin1String(Constants::MetaData::Track),       [validNum](const Track& track) { return validNum(track.trackNumber()); }},
        {QLatin1String(Constants::MetaData::TrackTotal),  [validNum](const Track& track) { return validNum(track.trackTotal()); }},
        {QLatin1String(Constants::MetaData::Disc),        [validNum](const Track& track) { return validNum(track.discNumber()); }},
        {QLatin1String(Constants::MetaData::DiscTotal),   [validNum](const Track& track) { return validNum(track.discTotal()); }},
        {QLatin1String(Constants::MetaData::Genre),       [](const Track& track) { return track.genre(); }},
        {QLatin1String(Constants::MetaData::Composer),    [](const Track& track) { return track.composer(); }},
        {QLatin1String(Constants::MetaData::Performer),   [](const Track& track) { return track.performer(); }},
        {QLatin1String(Constants::MetaData::Comment),     [](const Track& track) { return track.comment(); }},
        {QLatin1String(Constants::MetaData::Date),        [](const Track& track) { return track.date(); }},
        {QLatin1String(Constants::MetaData::Year),        [validNum](const Track& track) { return validNum(track.year()); }},
        {QLatin1String(Constants::MetaData::PlayCount),   [validNum](const Track& track) { return validNum(track.playCount()); }},
        {QLatin1String(Constants::MetaData::Rating),      [validNum](const Track& track) { return validNum(track.ratingStars()); }}
    };
    // clang-format on

    if(metaMap.contains(name)) {
        return metaMap.at(name)(*this);
    }

    return extraTag(name).join(u"\037");
}

void Track::setCuePath(const QString& path)
{
    p->cuePath = path;
}

void Track::addExtraTag(const QString& tag, const QString& value)
{
    if(tag.isEmpty() || value.isEmpty()) {
        return;
    }
    p->extraTags[tag].push_back(value);
}

void Track::removeExtraTag(const QString& tag)
{
    if(p->extraTags.contains(tag)) {
        p->removedTags.append(tag);
        p->extraTags.remove(tag);
    }
}

void Track::replaceExtraTag(const QString& tag, const QString& value)
{
    if(tag.isEmpty() || value.isEmpty()) {
        return;
    }

    if(value.isEmpty()) {
        removeExtraTag(tag);
    }
    else {
        p->extraTags[tag] = {value};
    }
}

void Track::clearExtraTags()
{
    p->extraTags.clear();
}

void Track::storeExtraTags(const QByteArray& tags)
{
    if(tags.isEmpty()) {
        return;
    }

    QByteArray in{tags};
    QDataStream stream(&in, QIODevice::ReadOnly);
    stream.setVersion(QDataStream::Qt_6_0);

    stream >> p->extraTags;
}

void Track::setOffset(uint64_t offset)
{
    p->offset = offset;
}

void Track::setDuration(uint64_t duration)
{
    p->duration = duration;
}

void Track::setFileSize(uint64_t fileSize)
{
    p->filesize = fileSize;
}

void Track::setBitrate(int rate)
{
    p->bitrate = rate;
}

void Track::setSampleRate(int rate)
{
    p->sampleRate = rate;
}

void Track::setChannels(int channels)
{
    if(channels > 0) {
        p->channels = channels;
    }
}

void Track::setBitDepth(int depth)
{
    p->bitDepth = depth;
}

void Track::setPlayCount(int count)
{
    p->playcount = count;
}

void Track::setAddedTime(uint64_t time)
{
    p->addedTime = time;
}

void Track::setModifiedTime(uint64_t time)
{
    if(p->modifiedTime > 0 && p->modifiedTime != time) {
        p->metadataWasModified = true;
    }
    p->modifiedTime = time;
}

void Track::setFirstPlayed(uint64_t time)
{
    if(p->firstPlayed == 0) {
        p->firstPlayed = time;
    }
}

void Track::setLastPlayed(uint64_t time)
{
    if(time > p->lastPlayed) {
        p->lastPlayed = time;
    }
}

void Track::setSort(const QString& sort)
{
    p->sort = sort;
}

void Track::clearWasModified()
{
    p->metadataWasModified = false;
}

QString Track::findCommonField(const TrackList& tracks)
{
    QString name;

    if(tracks.size() == 1) {
        name = tracks.front().title();
        if(name.isEmpty()) {
            name = tracks.front().filename();
        }
    }
    else {
        const QString primaryGenre  = tracks.front().genre();
        const QString primaryArtist = tracks.front().primaryAlbumArtist();
        const QString primaryAlbum  = tracks.front().album();

        const bool sameGenre  = std::all_of(tracks.cbegin(), tracks.cend(), [&primaryGenre](const Track& track) {
            return track.genre() == primaryGenre;
        });
        const bool sameArtist = std::all_of(tracks.cbegin(), tracks.cend(), [&primaryArtist](const Track& track) {
            return track.primaryAlbumArtist() == primaryArtist;
        });
        const bool sameAlbum  = std::all_of(tracks.cbegin(), tracks.cend(), [&primaryAlbum](const Track& track) {
            return track.album() == primaryAlbum;
        });

        if(sameArtist && sameAlbum) {
            name = QStringLiteral("%1 - %2").arg(primaryArtist, primaryAlbum);
        }
        else if(sameAlbum) {
            name = primaryAlbum;
        }
        else if(sameArtist) {
            name = primaryArtist;
        }
        else if(sameGenre) {
            name = primaryGenre;
        }
    }

    return name;
}

QStringList Track::supportedFileExtensions()
{
    static const QStringList supportedExtensions
        = {QStringLiteral("*.mp3"),  QStringLiteral("*.ogg"), QStringLiteral("*.opus"), QStringLiteral("*.oga"),
           QStringLiteral("*.m4a"),  QStringLiteral("*.wav"), QStringLiteral("*.wv"),   QStringLiteral("*.flac"),
           QStringLiteral("*.wma"),  QStringLiteral("*.mpc"), QStringLiteral("*.aiff"), QStringLiteral("*.ape"),
           QStringLiteral("*.webm"), QStringLiteral("*.mp4")};
    return supportedExtensions;
}

QStringList Track::supportedMimeTypes()
{
    static const QStringList supportedTypes = {QStringLiteral("audio/ogg"),
                                               QStringLiteral("audio/x-vorbis+ogg"),
                                               QStringLiteral("audio/mpeg"),
                                               QStringLiteral("audio/mpeg3"),
                                               QStringLiteral("audio/x-mpeg"),
                                               QStringLiteral("audio/x-aiff"),
                                               QStringLiteral("audio/x-aifc"),
                                               QStringLiteral("audio/vnd.wave"),
                                               QStringLiteral("audio/wav"),
                                               QStringLiteral("audio/x-wav"),
                                               QStringLiteral("audio/x-musepack"),
                                               QStringLiteral("audio/x-ape"),
                                               QStringLiteral("audio/x-wavpack"),
                                               QStringLiteral("audio/mp4"),
                                               QStringLiteral("audio/vnd.audible.aax"),
                                               QStringLiteral("audio/flac"),
                                               QStringLiteral("audio/ogg"),
                                               QStringLiteral("audio/x-vorbis+ogg"),
                                               QStringLiteral("audio/opus"),
                                               QStringLiteral("audio/x-opus+ogg"),
                                               QStringLiteral("audio/x-ms-wma")};
    return supportedTypes;
}

size_t qHash(const Track& track)
{
    return qHash(track.uniqueFilepath());
}
} // namespace Fooyin

QDataStream& operator<<(QDataStream& stream, const Fooyin::TrackIds& trackIds)
{
    stream << static_cast<int>(trackIds.size());

    for(const int id : trackIds) {
        stream << id;
    }
    return stream;
}

QDataStream& operator>>(QDataStream& stream, Fooyin::TrackIds& trackIds)
{
    int size;
    stream >> size;

    trackIds.reserve(size);

    while(size > 0) {
        --size;

        int trackId;
        stream >> trackId;
        trackIds.push_back(trackId);
    }
    return stream;
}
