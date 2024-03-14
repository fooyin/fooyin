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

#include <core/track.h>

#include <core/constants.h>
#include <utils/crypto.h>

#include <QFileInfo>
#include <QIODevice>

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
    QString filename;
    QString title;
    QStringList artists;
    QString album;
    QString albumArtist;
    int trackNumber{-1};
    int trackTotal{-1};
    int discNumber{-1};
    int discTotal{-1};
    QStringList genres;
    QString composer;
    QString performer;
    uint64_t duration{0};
    QString comment;
    QString date;
    int year{-1};
    QString coverPath;
    ExtraTags extraTags;
    QStringList removedTags;

    uint64_t filesize{0};
    int bitrate{0};
    int sampleRate{0};

    int playcount{0};

    uint64_t addedTime{0};
    uint64_t modifiedTime{0};
    uint64_t firstPlayed{0};
    uint64_t lastPlayed{0};

    QString sort;

    explicit Private(QString filepath_)
        : filepath{std::move(filepath_)}
    {
        filename = QFileInfo{this->filepath}.fileName();
    }
};

Track::Track()
    : Track{QStringLiteral("")}
{ }

Track::Track(QString filepath)
    : p{new Private(std::move(filepath))}
{ }

bool Track::operator==(const Track& other) const
{
    return filepath() == other.filepath() && hash() == other.hash();
}

bool Track::operator!=(const Track& other) const
{
    return filepath() != other.filepath() && hash() != other.hash();
}

Track::~Track()                             = default;
Track::Track(const Track& other)            = default;
Track& Track::operator=(const Track& other) = default;

QString Track::generateHash()
{
    p->hash = Utils::generateHash(p->artists.join(QStringLiteral(",")), p->album, QString::number(p->discNumber),
                                  QString::number(p->trackNumber), p->title);
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
    return QStringLiteral("%1|%2|%3")
        .arg(p->date, !p->albumArtist.isEmpty() ? p->albumArtist : p->artists.join(QStringLiteral("")), p->album);
}

Track::Type Track::type() const
{
    return p->type;
}

QString Track::typeString() const
{
    switch(p->type) {
        case(Type::MPEG):
            return QStringLiteral("MPEG");
        case(Type::AIFF):
            return QStringLiteral("AIFF");
        case(Type::WAV):
            return QStringLiteral("WAV");
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
            return QStringLiteral("OggOpus");
        case(Type::OggVorbis):
            return QStringLiteral("OggVorbis");
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

QString Track::relativePath() const
{
    return p->relativePath;
}

QString Track::filename() const
{
    return p->filename;
}

QString Track::title() const
{
    return p->title;
}

QStringList Track::artists() const
{
    return p->artists;
}

QString Track::artist() const
{
    return p->artists.join(u"\037");
}

QString Track::album() const
{
    return p->album;
}

QString Track::albumArtist() const
{
    return p->albumArtist;
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
    return p->genres.join(u"\037");
}

QString Track::composer() const
{
    return p->composer;
}

QString Track::performer() const
{
    return p->performer;
}

uint64_t Track::duration() const
{
    return p->duration;
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

QString Track::coverPath() const
{
    return p->coverPath;
}

bool Track::hasCover() const
{
    return !p->coverPath.isEmpty();
}

bool Track::hasEmbeddedCover() const
{
    return p->coverPath == u"|Embedded|";
}

QStringList Track::extraTag(const QString& tag) const
{
    if(p->extraTags.contains(tag)) {
        return p->extraTags.value(tag);
    }
    return {};
}

ExtraTags Track::extraTags() const
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
    p->filename = QFileInfo{path}.fileName();
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
    p->artists = artists;

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

void Track::setAlbumArtist(const QString& artist)
{
    p->albumArtist = artist;
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
    p->genres = genres;
}

void Track::setComposer(const QString& composer)
{
    p->composer = composer;
}

void Track::setPerformer(const QString& performer)
{
    p->performer = performer;
}

void Track::setDuration(uint64_t duration)
{
    p->duration = duration;
}

void Track::setComment(const QString& comment)
{
    p->comment = comment;
}

void Track::setDate(const QString& date)
{
    p->date = date;

    const QStringList dateParts = date.split(QChar::fromLatin1('-'));
    if(dateParts.empty()) {
        if(date.length() == 4) {
            p->year = p->date.toInt();
        }
    }
    else {
        p->year = dateParts.front().toInt();
    }
}

void Track::setYear(int year)
{
    p->year = year;
}

void Track::setCoverPath(const QString& path)
{
    p->coverPath = path;
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

QStringList Track::supportedFileExtensions()
{
    static const QStringList supportedExtensions
        = {QStringLiteral("*.mp3"), QStringLiteral("*.ogg"),  QStringLiteral("*.opus"), QStringLiteral("*.oga"),
           QStringLiteral("*.m4a"), QStringLiteral("*.wav"),  QStringLiteral("*.flac"), QStringLiteral("*.wma"),
           QStringLiteral("*.mpc"), QStringLiteral("*.aiff"), QStringLiteral("*.ape"),  QStringLiteral("*.webm"),
           QStringLiteral("*.mp4")};
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
    return qHash(track.filepath());
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
