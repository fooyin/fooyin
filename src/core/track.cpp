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
class TrackPrivate : public QSharedData
{
public:
    int libraryId{-1};
    bool enabled{true};
    int id{-1};
    QString hash;
    QString codec;
    QString filepath;
    QString directory;
    QString filename;
    QString extension;
    QString title;
    QStringList artists;
    QString album;
    QStringList albumArtists;
    QString trackNumber;
    QString trackTotal;
    QString discNumber;
    QString discTotal;
    QStringList genres;
    QString composer;
    QString performer;
    QString comment;
    QString date;
    int year{-1};
    Track::ExtraTags extraTags;
    QStringList removedTags;

    QString cuePath;

    int subsong{0};
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

    // Archive related
    bool isInArchive{false};
    QString archivePath;
    QString filepathWithinArchive;

    void splitArchiveUrl()
    {
        QString path = filepath.mid(filepath.indexOf(u"://") + 3);
        path         = path.mid(path.indexOf(u'|') + 1);

        const auto lengthEndIndex   = path.indexOf(u'|');
        const int archivePathLength = path.left(lengthEndIndex).toInt();
        path                        = path.mid(lengthEndIndex + 1);

        const QString filePrefix = QStringLiteral("file://");
        path                     = path.sliced(filePrefix.length());

        archivePath           = path.left(archivePathLength);
        filepathWithinArchive = path.mid(archivePathLength + 1);

        const QFileInfo info{filepathWithinArchive};
        filename  = info.completeBaseName();
        extension = info.suffix().toLower();
        directory = info.dir().dirName();
        if(directory == u".") {
            directory = QFileInfo{archivePath}.fileName();
        }
    }
};

Track::Track()
    : Track{{}}
{ }

Track::Track(const QString& filepath)
    : p{new TrackPrivate()}
{
    setFilePath(filepath);
}

Track::Track(const QString& filepath, int subsong)
    : Track{filepath}
{
    setSubsong(subsong);
}

Track::~Track()                             = default;
Track::Track(const Track& other)            = default;
Track& Track::operator=(const Track& other) = default;

bool Track::operator==(const Track& other) const
{
    return uniqueFilepath() == other.uniqueFilepath() && duration() == other.duration() && hash() == other.hash();
}

bool Track::operator!=(const Track& other) const
{
    return !(*this == other);
}

bool Track::operator<(const Track& other) const
{
    return uniqueFilepath() < other.uniqueFilepath();
}

QString Track::generateHash()
{
    QString title = p->title;
    if(title.isEmpty()) {
        title = p->directory + p->filename;
    }

    p->hash = Utils::generateHash(p->artists.join(QStringLiteral(",")), p->album, p->discNumber, p->trackNumber, title,
                                  QString::number(p->subsong));
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
    return p->filesize > 0 && p->modifiedTime > 0;
}

bool Track::metadataWasModified() const
{
    return p->metadataWasModified;
}

bool Track::exists() const
{
    if(isInArchive()) {
        return QFileInfo::exists(archivePath());
    }
    return QFileInfo::exists(filepath());
}

int Track::libraryId() const
{
    return p->libraryId;
}

bool Track::isInArchive() const
{
    return p->isInArchive;
}

QString Track::archivePath() const
{
    return p->archivePath;
}

QString Track::pathInArchive() const
{
    return p->filepathWithinArchive;
}

QString Track::relativeArchivePath() const
{
    return QFileInfo{p->filepathWithinArchive}.path();
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

QString Track::prettyFilepath() const
{
    if(isInArchive()) {
        return archivePath() + u"/" + pathInArchive();
    }

    return p->filepath;
}

QString Track::filename() const
{
    return p->filename;
}

QString Track::path() const
{
    if(isInArchive()) {
        return QFileInfo{prettyFilepath()}.dir().path();
    }

    return QFileInfo{p->filepath}.absolutePath();
}

QString Track::directory() const
{
    return p->directory;
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
    return p->artists.empty() ? QString{} : p->artists.join(u"\037");
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
    return uniqArtists.isEmpty() ? QString{} : uniqArtists.join(u"\037");
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
    return p->albumArtists.empty() ? QString{} : p->albumArtists.join(u"\037");
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

QString Track::trackNumber() const
{
    return p->trackNumber;
}

QString Track::trackTotal() const
{
    return p->trackTotal;
}

QString Track::discNumber() const
{
    return p->discNumber;
}

QString Track::discTotal() const
{
    return p->discTotal;
}

QStringList Track::genres() const
{
    return p->genres;
}

QString Track::genre() const
{
    return p->genres.empty() ? QString{} : p->genres.join(u"\037");
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

int Track::subsong() const
{
    return p->subsong;
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

QString Track::codec() const
{
    return p->codec;
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

void Track::setCodec(const QString& codec)
{
    p->codec = codec;
}

void Track::setFilePath(const QString& path)
{
    if(path.isEmpty()) {
        return;
    }

    p->filepath = path;

    if(path.first(9) == u"unpack://") {
        p->isInArchive = true;
        p->splitArchiveUrl();
    }
    else {
        p->isInArchive = false;
        const QFileInfo info{p->filepath};
        p->filename  = info.completeBaseName();
        p->extension = info.suffix().toLower();
        p->directory = info.dir().dirName();
    }
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

void Track::setTrackNumber(const QString& number)
{
    if(number == u"-1") {
        return;
    }

    p->trackNumber = number;

    if(!p->hash.isEmpty()) {
        generateHash();
    }
}

void Track::setTrackTotal(const QString& total)
{
    if(total == u"-1") {
        return;
    }

    p->trackTotal = total;
}

void Track::setDiscNumber(const QString& number)
{
    if(number == u"-1") {
        return;
    }

    p->discNumber = number;

    if(!p->hash.isEmpty()) {
        generateHash();
    }
}

void Track::setDiscTotal(const QString& total)
{
    if(total == u"-1") {
        return;
    }

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
    if(rating > 0 && rating <= 1.0) {
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
        {QLatin1String(Constants::MetaData::Title),       [](const Track& track) { return track.title(); }},
        {QLatin1String(Constants::MetaData::Artist),      [](const Track& track) { return track.artist(); }},
        {QLatin1String(Constants::MetaData::Album),       [](const Track& track) { return track.album(); }},
        {QLatin1String(Constants::MetaData::AlbumArtist), [](const Track& track) { return track.albumArtist(); }},
        {QLatin1String(Constants::MetaData::Track),       [](const Track& track) { return track.trackNumber(); }},
        {QLatin1String(Constants::MetaData::TrackTotal),  [](const Track& track) { return track.trackTotal(); }},
        {QLatin1String(Constants::MetaData::Disc),        [](const Track& track) { return track.discNumber(); }},
        {QLatin1String(Constants::MetaData::DiscTotal),   [](const Track& track) { return track.discTotal(); }},
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

QString Track::techInfo(const QString& name) const
{
    auto validNum = [](auto num) -> QString {
        if(num > 0) {
            return QString::number(num);
        }
        return {};
    };

    // clang-format off
    static const std::unordered_map<QString, std::function<QString(const Track& track)>> infoMap{
        {QLatin1String(Constants::MetaData::Codec),      [](const Track& track) { return track.codec(); }},
        {QLatin1String(Constants::MetaData::SampleRate), [validNum](const Track& track) { return validNum(track.sampleRate()); }},
        {QLatin1String(Constants::MetaData::Channels),   [validNum](const Track& track) { return validNum(track.channels()); }},
        {QLatin1String(Constants::MetaData::BitDepth),   [validNum](const Track& track) { return validNum(track.bitDepth()); }},
        {QLatin1String(Constants::MetaData::Duration),   [validNum](const Track& track) { return validNum(track.duration()); }}
    };
    // clang-format on

    if(infoMap.contains(name)) {
        return infoMap.at(name)(*this);
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
    p->extraTags[tag.toUpper()].push_back(value);
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

void Track::setSubsong(int index)
{
    if(index >= 0) {
        p->subsong = index;
    }
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
        const QString primaryDir    = tracks.front().directory();

        const bool sameGenre  = std::all_of(tracks.cbegin(), tracks.cend(), [&primaryGenre](const Track& track) {
            return track.genre() == primaryGenre;
        });
        const bool sameArtist = std::all_of(tracks.cbegin(), tracks.cend(), [&primaryArtist](const Track& track) {
            return track.primaryAlbumArtist() == primaryArtist;
        });
        const bool sameAlbum  = std::all_of(tracks.cbegin(), tracks.cend(), [&primaryAlbum](const Track& track) {
            return track.album() == primaryAlbum;
        });
        const bool sameDir    = std::all_of(tracks.cbegin(), tracks.cend(),
                                            [&primaryDir](const Track& track) { return track.directory() == primaryDir; });

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
        else if(sameDir) {
            name = primaryDir;
        }
    }

    return name;
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
