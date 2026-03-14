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
#include <core/stringpool.h>
#include <core/track.h>
#include <core/trackmetadatastore.h>

#include <utils/crypto.h>

#include <QDir>
#include <QFileInfo>
#include <QIODevice>
#include <QRegularExpression>

#include <chrono>

using namespace Qt::StringLiterals;

constexpr auto MaxStarCount   = 10;
constexpr auto YearRegex      = R"lit(\b\d{4}\b)lit";
constexpr auto YearMonthRegex = R"lit(\b(\d{4})-(\d{2})\b)lit";
constexpr auto FullDateRegex  = R"lit(\b(\d{4})-(\d{2})-(\d{2})\b)lit";

namespace {
QString validNum(auto num)
{
    if(num > 0) {
        return QString::number(num);
    }
    return {};
};

using MetaMap = std::unordered_map<QString, std::function<QString(const Fooyin::Track& track)>>;
const MetaMap& metaMap()
{
    using namespace Fooyin::Constants::MetaData;
    // clang-format off
    static const MetaMap metaMap{
        {QString::fromLatin1(Title),        [](const Fooyin::Track& track) { return track.title(); }},
        {QString::fromLatin1(Artist),       [](const Fooyin::Track& track) { return track.artist(); }},
        {QString::fromLatin1(Album),        [](const Fooyin::Track& track) { return track.album(); }},
        {QString::fromLatin1(AlbumArtist),  [](const Fooyin::Track& track) { return track.albumArtist(); }},
        {QString::fromLatin1(Track),        [](const Fooyin::Track& track) { return track.trackNumber(); }},
        {QString::fromLatin1(TrackTotal),   [](const Fooyin::Track& track) { return track.trackTotal(); }},
        {QString::fromLatin1(Disc),         [](const Fooyin::Track& track) { return track.discNumber(); }},
        {QString::fromLatin1(DiscTotal),    [](const Fooyin::Track& track) { return track.discTotal(); }},
        {QString::fromLatin1(Genre),        [](const Fooyin::Track& track) { return track.genre(); }},
        {QString::fromLatin1(Composer),     [](const Fooyin::Track& track) { return track.composer(); }},
        {QString::fromLatin1(Performer),    [](const Fooyin::Track& track) { return track.performer(); }},
        {QString::fromLatin1(Comment),      [](const Fooyin::Track& track) { return track.comment(); }},
        {QString::fromLatin1(Date),         [](const Fooyin::Track& track) { return track.date(); }},
        {QString::fromLatin1(Year),         [](const Fooyin::Track& track) { return validNum(track.year()); }},
        {QString::fromLatin1(Rating),       [](const Fooyin::Track& track) { return validNum(track.rating()); }},
        {QString::fromLatin1(RatingEditor), [](const Fooyin::Track& track) { return validNum(track.rating()); }},
        {QString::fromLatin1(RatingStars),  [](const Fooyin::Track& track) { return validNum(track.ratingStars()); }}
    };
    // clang-format on
    return metaMap;
}

/*!
 * Dense in-memory storage for decoded extra tags.
 *
 * Extra tags are usually cold metadata and many tracks never decode them at all.
 * When they are decoded, keeping them in a `QMap<QString, QStringList>` is
 * unnecessarily expensive for the common case of only a few keys per track.
 *
 * This structure keeps the resident form compact by storing key/value pairs in
 * insertion order inside a flat vector. Lookups are linear by design; the
 * expected key count per track is small enough that the lower memory overhead is
 * a better trade than node-based map storage.
 *
 * Conversion back to `Track::ExtraTags` is kept at the API/serialization
 * boundary only.
 */
class CompactExtraTags
{
public:
    struct Entry
    {
        QString key;
        QStringList values;
    };

    [[nodiscard]] bool contains(const QString& key) const
    {
        return findIndex(key) >= 0;
    }

    [[nodiscard]] QStringList value(const QString& key) const
    {
        if(const auto index = findIndex(key); index >= 0) {
            return m_entries.at(index).values;
        }
        return {};
    }

    [[nodiscard]] bool empty() const
    {
        return m_entries.empty();
    }

    void clear()
    {
        m_entries.clear();
    }

    void reserve(qsizetype size)
    {
        m_entries.reserve(size);
    }

    QStringList& valuesFor(const QString& key)
    {
        if(const auto index = findIndex(key); index >= 0) {
            return m_entries.at(index).values;
        }

        m_entries.emplace_back(Entry{key, {}});
        return m_entries.back().values;
    }

    void setValues(const QString& key, const QStringList& values)
    {
        valuesFor(key) = values;
    }

    bool remove(const QString& key)
    {
        if(const auto index = findIndex(key); index >= 0) {
            m_entries.erase(m_entries.begin() + index);
            return true;
        }
        return false;
    }

    [[nodiscard]] Fooyin::Track::ExtraTags toMap() const
    {
        Fooyin::Track::ExtraTags tags;
        for(const auto& entry : m_entries) {
            tags.insert(entry.key, entry.values);
        }
        return tags;
    }

private:
    [[nodiscard]] qsizetype findIndex(const QString& key) const
    {
        for(qsizetype index{0}; index < static_cast<qsizetype>(m_entries.size()); ++index) {
            if(m_entries.at(index).key == key) {
                return index;
            }
        }
        return -1;
    }

    std::vector<Entry> m_entries;
};

} // namespace

namespace Fooyin {
class TrackPrivate : public QSharedData
{
public:
    void splitArchiveUrl();
    [[nodiscard]] QString directory() const;
    [[nodiscard]] QString filename() const;
    [[nodiscard]] QString extension() const;

    void ensureExtraTagsLoaded() const;
    [[nodiscard]] StringPool& stringPool() const;

    std::shared_ptr<TrackMetadataStore> metadataStore{std::make_shared<TrackMetadataStore>()};
    int libraryId{-1};
    bool enabled{true};
    int id{-1};
    QString hash;
    StringPool::StringId codec{StringPool::EmptyStringId};
    QString filepath;
    QString title;
    StringPool::StringListRef artists;
    StringPool::StringId album{StringPool::EmptyStringId};
    StringPool::StringListRef albumArtists;
    QString trackNumber;
    QString trackTotal;
    QString discNumber;
    QString discTotal;
    StringPool::StringListRef genres;
    StringPool::StringListRef composers;
    StringPool::StringListRef performers;
    QString comment;
    QString date;
    int year{-1};
    int64_t dateSinceEpoch;
    int64_t yearSinceEpoch;
    mutable CompactExtraTags extraTags;
    mutable QByteArray extraTagsBlob;
    mutable bool extraTagsLoaded{true};
    bool extraTagsDirty{false};
    QStringList removedTags;
    Track::ExtraProperties extraProps;

    QString cuePath;

    int subsong{0};
    uint64_t offset{0};
    uint64_t duration{0};
    uint64_t filesize{0};
    int bitrate{0};
    int sampleRate{0};
    int channels{2};
    int bitDepth{-1};
    QString codecProfile;
    QString tool;
    QStringList tagTypes;
    StringPool::StringId encoding{StringPool::EmptyStringId};

    float rating{-1};
    int playcount{0};
    uint64_t addedTime{0};
    uint64_t modifiedTime{0};
    uint64_t firstPlayed{0};
    uint64_t lastPlayed{0};

    float rgTrackGain{Constants::InvalidGain};
    float rgAlbumGain{Constants::InvalidGain};
    float rgTrackPeak{Constants::InvalidPeak};
    float rgAlbumPeak{Constants::InvalidPeak};

    bool metadataWasRead{false};
    bool metadataWasModified{false};

    // Archive related
    bool isInArchive{false};
    QString archivePath;
    QString filepathWithinArchive;
};

void TrackPrivate::splitArchiveUrl()
{
    QString path = filepath.mid(filepath.indexOf("://"_L1) + 3);
    path         = path.mid(path.indexOf(u'|') + 1);

    const auto lengthEndIndex   = path.indexOf(u'|');
    const int archivePathLength = path.left(lengthEndIndex).toInt();
    path                        = path.mid(lengthEndIndex + 1);

    const QString filePrefix = u"file://"_s;
    path                     = path.sliced(filePrefix.length());

    archivePath           = path.left(archivePathLength);
    filepathWithinArchive = path.mid(archivePathLength + 1);
}

QString TrackPrivate::directory() const
{
    const QFileInfo info{isInArchive ? filepathWithinArchive : filepath};
    QString dir = info.dir().dirName();
    if(isInArchive && dir == "."_L1) {
        dir = QFileInfo{archivePath}.fileName();
    }
    return dir;
}

QString TrackPrivate::filename() const
{
    return QFileInfo{isInArchive ? filepathWithinArchive : filepath}.completeBaseName();
}

QString TrackPrivate::extension() const
{
    return QFileInfo{isInArchive ? filepathWithinArchive : filepath}.suffix().toLower();
}

void TrackPrivate::ensureExtraTagsLoaded() const
{
    if(extraTagsLoaded) {
        return;
    }

    extraTagsLoaded = true;
    extraTags.clear();

    if(extraTagsBlob.isEmpty()) {
        return;
    }

    QByteArray in{extraTagsBlob};
    QDataStream stream(&in, QIODevice::ReadOnly);
    stream.setVersion(QDataStream::Qt_6_0);

    Track::ExtraTags loaded;
    stream >> loaded;
    extraTags.reserve(loaded.size());

    for(auto it = loaded.cbegin(); it != loaded.cend(); ++it) {
        extraTags.setValues(stringPool().intern(StringPool::Domain::ExtraTagKey, it.key().toUpper()), it.value());
    }
}

StringPool& TrackPrivate::stringPool() const
{
    return metadataStore->stringPool();
}

QString internString(const TrackPrivate& track, StringPool::Domain domain, const QString& value)
{
    return track.stringPool().intern(domain, value);
}

Fooyin::StringPool::StringId internStringId(const TrackPrivate& track, StringPool::Domain domain, const QString& value)
{
    return track.stringPool().internId(domain, value);
}

Fooyin::StringPool::StringListRef internStrings(const TrackPrivate& track, StringPool::Domain domain,
                                                const QStringList& values)
{
    return track.stringPool().internList(domain, values);
}

QString resolveString(const TrackPrivate& track, StringPool::Domain domain, StringPool::StringId id)
{
    return track.stringPool().resolve(domain, id);
}

QStringList resolveStrings(const TrackPrivate& track, StringPool::Domain domain, StringPool::StringListRef ref)
{
    return track.stringPool().resolveList(domain, ref);
}

QString joinStrings(const TrackPrivate& track, StringPool::Domain domain, StringPool::StringListRef ref,
                    const QString& separator)
{
    return track.stringPool().joined(domain, ref, separator);
}

QString stringAt(const TrackPrivate& track, StringPool::Domain domain, StringPool::StringListRef ref, qsizetype index)
{
    return track.stringPool().valueAt(domain, ref, index);
}

bool containsString(const TrackPrivate& track, StringPool::Domain domain, StringPool::StringListRef ref,
                    const QString& value)
{
    return track.stringPool().contains(domain, ref, value);
}

QString internExtraTagKey(const TrackPrivate& track, const QString& tag)
{
    return internString(track, StringPool::Domain::ExtraTagKey, tag.toUpper());
}

CompactExtraTags internExtraTags(const TrackPrivate& track, const Track::ExtraTags& tags)
{
    CompactExtraTags interned;
    interned.reserve(tags.size());

    for(auto it = tags.cbegin(); it != tags.cend(); ++it) {
        interned.setValues(internExtraTagKey(track, it.key()), it.value());
    }

    return interned;
}

Track::Track()
    : Track{std::shared_ptr<TrackMetadataStore>{}}
{ }

Track::Track(std::shared_ptr<TrackMetadataStore> store)
    : p{new TrackPrivate()}
{
    p->metadataStore = store ? std::move(store) : std::make_shared<TrackMetadataStore>();
}

Track::Track(const QString& filepath)
    : Track{filepath, std::shared_ptr<TrackMetadataStore>{}}
{ }

Track::Track(const QString& filepath, std::shared_ptr<TrackMetadataStore> store)
    : Track{std::move(store)}
{
    setFilePath(filepath);
}

Track::Track(const QString& filepath, int subsong)
    : Track{filepath, subsong, std::shared_ptr<TrackMetadataStore>{}}
{ }

Track::Track(const QString& filepath, int subsong, std::shared_ptr<TrackMetadataStore> store)
    : Track{filepath, std::move(store)}
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
        title = p->directory() + p->filename();
    }

    p->hash = Utils::generateHash(joinStrings(*p, StringPool::Domain::Artist, p->artists, ","_L1),
                                  resolveString(*p, StringPool::Domain::Album, p->album), p->discNumber, p->trackNumber,
                                  title, QString::number(p->subsong));
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
    return p->metadataWasRead;
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

    const auto albumArtists = this->albumArtists();
    const auto artists      = this->artists();
    const auto album        = this->album();

    if(!p->date.isEmpty()) {
        hash.append(p->date);
    }
    if(!albumArtists.isEmpty()) {
        hash.append(albumArtists.join(","_L1));
    }
    if(!artists.isEmpty()) {
        hash.append(artists.join(","_L1));
    }

    if(!album.isEmpty()) {
        hash.append(album);
    }
    else {
        hash.append(p->directory());
    }

    return hash.join("|"_L1);
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
        return archivePath() + "/"_L1 + pathInArchive();
    }

    return p->filepath;
}

QString Track::filename() const
{
    return p->filename();
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
    return p->directory();
}

QString Track::extension() const
{
    return p->extension();
}

QString Track::filenameExt() const
{
    return QFileInfo{p->filepath}.fileName();
}

QString Track::title() const
{
    return p->title;
}

QString Track::effectiveTitle() const
{
    return !p->title.isEmpty() ? p->title : p->filename();
}

bool Track::hasArtists() const
{
    return !p->artists.isEmpty();
}

qsizetype Track::artistCount() const
{
    return p->artists.size;
}

QString Track::artistAt(qsizetype index) const
{
    return stringAt(*p, StringPool::Domain::Artist, p->artists, index);
}

QStringList Track::artists() const
{
    return resolveStrings(*p, StringPool::Domain::Artist, p->artists);
}

QString Track::artistsJoined(const QString& sep) const
{
    return joinStrings(*p, StringPool::Domain::Artist, p->artists, sep);
}

QStringList Track::uniqueArtists() const
{
    QStringList uniqueArtists;
    uniqueArtists.reserve(artistCount());

    for(qsizetype index{0}; index < artistCount(); ++index) {
        const QString artist = artistAt(index);
        if(!containsString(*p, StringPool::Domain::AlbumArtist, p->albumArtists, artist)) {
            uniqueArtists.emplace_back(artist);
        }
    }
    return uniqueArtists;
}

QString Track::artist() const
{
    return artistsJoined(QLatin1String{Constants::UnitSeparator});
}

QString Track::primaryArtist() const
{
    if(hasArtists()) {
        return artist();
    }
    if(hasAlbumArtists()) {
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
    return uniqArtists.isEmpty() ? QString{} : uniqArtists.join(QLatin1String{Constants::UnitSeparator});
}

QString Track::album() const
{
    return resolveString(*p, StringPool::Domain::Album, p->album);
}

bool Track::hasAlbumArtists() const
{
    return !p->albumArtists.isEmpty();
}

qsizetype Track::albumArtistCount() const
{
    return p->albumArtists.size;
}

QString Track::albumArtistAt(qsizetype index) const
{
    return stringAt(*p, StringPool::Domain::AlbumArtist, p->albumArtists, index);
}

QStringList Track::albumArtists() const
{
    return resolveStrings(*p, StringPool::Domain::AlbumArtist, p->albumArtists);
}

QString Track::albumArtistsJoined(const QString& sep) const
{
    return joinStrings(*p, StringPool::Domain::AlbumArtist, p->albumArtists, sep);
}

QString Track::albumArtist() const
{
    return albumArtistsJoined(QLatin1String{Constants::UnitSeparator});
}

QString Track::effectiveAlbumArtist(bool useVarious) const
{
    if(hasAlbumArtists()) {
        return albumArtist();
    }
    if(useVarious && hasExtraTag(u"COMPILATION"_s)) {
        const auto compilation = extraTag(u"COMPILATION"_s);
        if(!compilation.empty() && compilation.front().toInt() == 1) {
            return u"Various Artists"_s;
        }
    }
    if(hasArtists()) {
        return artist();
    }
    if(!composers().isEmpty()) {
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

bool Track::hasGenres() const
{
    return !p->genres.isEmpty();
}

qsizetype Track::genreCount() const
{
    return p->genres.size;
}

QString Track::genreAt(qsizetype index) const
{
    return stringAt(*p, StringPool::Domain::Genre, p->genres, index);
}

QStringList Track::genres() const
{
    return resolveStrings(*p, StringPool::Domain::Genre, p->genres);
}

QString Track::genresJoined(const QString& sep) const
{
    return joinStrings(*p, StringPool::Domain::Genre, p->genres, sep);
}

QString Track::genre() const
{
    return genresJoined(QLatin1String{Constants::UnitSeparator});
}

QStringList Track::composers() const
{
    return resolveStrings(*p, StringPool::Domain::Composer, p->composers);
}

QString Track::composer() const
{
    return joinStrings(*p, StringPool::Domain::Composer, p->composers, QLatin1String{Constants::UnitSeparator});
}

QStringList Track::performers() const
{
    return resolveStrings(*p, StringPool::Domain::Performer, p->performers);
}

QString Track::performer() const
{
    return joinStrings(*p, StringPool::Domain::Performer, p->performers, QLatin1String{Constants::UnitSeparator});
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

bool Track::hasRGInfo() const
{
    return hasTrackGain() || hasAlbumGain() || hasTrackPeak() || hasAlbumPeak();
}

bool Track::hasTrackGain() const
{
    return p->rgTrackGain != Constants::InvalidGain;
}

bool Track::hasAlbumGain() const
{
    return p->rgAlbumGain != Constants::InvalidGain;
}

bool Track::hasTrackPeak() const
{
    return p->rgTrackPeak != Constants::InvalidPeak;
}

bool Track::hasAlbumPeak() const
{
    return p->rgAlbumPeak != Constants::InvalidPeak;
}

float Track::rgTrackGain() const
{
    return p->rgTrackGain;
}

float Track::rgAlbumGain() const
{
    return p->rgAlbumGain;
}

float Track::rgTrackPeak() const
{
    return p->rgTrackPeak;
}

float Track::rgAlbumPeak() const
{
    return p->rgAlbumPeak;
}

bool Track::hasCue() const
{
    return !p->cuePath.isEmpty();
}

bool Track::hasEmbeddedCue() const
{
    return cuePath() == "Embedded"_L1;
}

QString Track::cuePath() const
{
    return p->cuePath;
}

bool Track::isArchivePath(const QString& path)
{
    return path.startsWith("unpack://"_L1);
}

bool Track::isMultiValueTag(const QString& tag)
{
    const QString trackTag = tag.toUpper();
    return trackTag == QLatin1String{Constants::MetaData::Artist}
        || trackTag == QLatin1String{Constants::MetaData::AlbumArtist}
        || trackTag == QLatin1String{Constants::MetaData::Genre}
        || trackTag == QLatin1String{Constants::MetaData::Composer}
        || trackTag == QLatin1String{Constants::MetaData::Performer};
}

bool Track::isExtraTag(const QString& tag)
{
    const QString trackTag = tag.toUpper();

    const auto& map = metaMap();
    return !map.contains(trackTag);
}

bool Track::hasExtraTag(const QString& tag) const
{
    p->ensureExtraTagsLoaded();
    return p->extraTags.contains(tag);
}

QStringList Track::extraTag(const QString& tag) const
{
    p->ensureExtraTagsLoaded();
    if(p->extraTags.contains(tag)) {
        return p->extraTags.value(tag);
    }
    return {};
}

Track::ExtraTags Track::extraTags() const
{
    p->ensureExtraTagsLoaded();
    return p->extraTags.toMap();
}

QStringList Track::removedTags() const
{
    return p->removedTags;
}

QByteArray Track::serialiseExtraTags() const
{
    if(!p->extraTagsDirty && !p->extraTagsLoaded) {
        return p->extraTagsBlob;
    }

    p->ensureExtraTagsLoaded();

    if(p->extraTags.empty()) {
        return {};
    }

    QByteArray out;
    QDataStream stream(&out, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);

    stream << p->extraTags.toMap();

    return out;
}

QMap<QString, QString> Track::metadata() const
{
    QMap<QString, QString> map;

    const auto addField = [&map]<typename T>(const QString& title, const T& field) {
        if constexpr(std::is_same_v<T, QString>) {
            if(!field.isEmpty()) {
                map[title] = field;
            }
        }
        if constexpr(std::is_same_v<T, QStringList>) {
            if(!field.isEmpty()) {
                map[title] = field.join("; "_L1);
            }
        }
    };

    using namespace Constants;

    static const QString TitleKey       = QString::fromLatin1(MetaData::Title);
    static const QString ArtistKey      = QString::fromLatin1(MetaData::Artist);
    static const QString AlbumKey       = QString::fromLatin1(MetaData::Album);
    static const QString AlbumArtistKey = QString::fromLatin1(MetaData::AlbumArtist);
    static const QString TrackKey       = QString::fromLatin1(MetaData::Track);
    static const QString TrackTotalKey  = QString::fromLatin1(MetaData::TrackTotal);
    static const QString DiscKey        = QString::fromLatin1(MetaData::Disc);
    static const QString DiscTotalKey   = QString::fromLatin1(MetaData::DiscTotal);
    static const QString GenreKey       = QString::fromLatin1(MetaData::Genre);
    static const QString ComposerKey    = QString::fromLatin1(MetaData::Composer);
    static const QString PerformerKey   = QString::fromLatin1(MetaData::Performer);
    static const QString CommentKey     = QString::fromLatin1(MetaData::Comment);
    static const QString DateKey        = QString::fromLatin1(MetaData::Date);

    addField(TitleKey, p->title);
    addField(ArtistKey, artists());
    addField(AlbumKey, album());
    addField(AlbumArtistKey, albumArtists());
    addField(TrackKey, p->trackNumber);
    addField(TrackTotalKey, p->trackTotal);
    addField(DiscKey, p->discNumber);
    addField(DiscTotalKey, p->discTotal);
    addField(GenreKey, genres());
    addField(ComposerKey, composers());
    addField(PerformerKey, performers());
    addField(CommentKey, p->comment);
    addField(DateKey, p->date);

    return map;
}

bool Track::hasExtraProperty(const QString& prop) const
{
    return p->extraProps.contains(prop);
}

Track::ExtraProperties Track::extraProperties() const
{
    return p->extraProps;
}

QByteArray Track::serialiseExtraProperties() const
{
    if(p->extraProps.empty()) {
        return {};
    }

    QByteArray out;
    QDataStream stream(&out, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);

    stream << p->extraProps;

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
    return resolveString(*p, StringPool::Domain::Codec, p->codec);
}

QString Track::codecProfile() const
{
    return p->codecProfile;
}

QString Track::tool() const
{
    return p->tool;
}

QString Track::tagType(const QString& sep) const
{
    return p->tagTypes.empty() ? QString{}
                               : p->tagTypes.join(!sep.isEmpty() ? sep : QLatin1String{Constants::UnitSeparator});
}

QStringList Track::tagTypes() const
{
    return p->tagTypes;
}

QString Track::encoding() const
{
    return resolveString(*p, StringPool::Domain::Encoding, p->encoding);
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

uint64_t Track::lastModified() const
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

bool Track::hasMatch(const QString& term) const
{
    const auto contains = [&term](const QString& text) {
        return text.contains(term, Qt::CaseInsensitive);
    };

    // clang-format off
    return contains(artist()) ||
           contains(title()) ||
           contains(album()) ||
           contains(albumArtist()) ||
           contains(performer()) ||
           contains(composer()) ||
           contains(genre()) ||
           contains(filepath());
    // clang-format on
}

std::shared_ptr<TrackMetadataStore> Track::metadataStore() const
{
    return p->metadataStore;
}

void Track::setMetadataStore(std::shared_ptr<TrackMetadataStore> store)
{
    if(!store) {
        store = std::make_shared<TrackMetadataStore>();
    }

    if(p->metadataStore == store) {
        return;
    }

    p.detach();

    const auto oldStore = p->metadataStore;

    const auto resolveOld = [&oldStore](StringPool::Domain domain, StringPool::StringId id) {
        return oldStore ? oldStore->stringPool().resolve(domain, id) : QString{};
    };
    const auto resolveOldList = [&oldStore](StringPool::Domain domain, StringPool::StringListRef ref) {
        return oldStore ? oldStore->stringPool().resolveList(domain, ref) : QStringList{};
    };

    p->codec = store->stringPool().internId(StringPool::Domain::Codec, resolveOld(StringPool::Domain::Codec, p->codec));
    p->artists = store->stringPool().internList(StringPool::Domain::Artist,
                                                resolveOldList(StringPool::Domain::Artist, p->artists));
    p->album = store->stringPool().internId(StringPool::Domain::Album, resolveOld(StringPool::Domain::Album, p->album));
    p->albumArtists = store->stringPool().internList(StringPool::Domain::AlbumArtist,
                                                     resolveOldList(StringPool::Domain::AlbumArtist, p->albumArtists));
    p->genres       = store->stringPool().internList(StringPool::Domain::Genre,
                                                     resolveOldList(StringPool::Domain::Genre, p->genres));
    p->composers    = store->stringPool().internList(StringPool::Domain::Composer,
                                                     resolveOldList(StringPool::Domain::Composer, p->composers));
    p->performers   = store->stringPool().internList(StringPool::Domain::Performer,
                                                     resolveOldList(StringPool::Domain::Performer, p->performers));
    p->encoding     = store->stringPool().internId(StringPool::Domain::Encoding,
                                                   resolveOld(StringPool::Domain::Encoding, p->encoding));

    p->metadataStore = store;

    if(p->extraTagsLoaded && !p->extraTags.empty()) {
        p->extraTags = internExtraTags(*p, p->extraTags.toMap());
    }
}

void Track::setLibraryId(int id)
{
    p->libraryId = id;
}

void Track::setIsEnabled(bool enabled)
{
    p->enabled = enabled;
}

void Track::setMetadataWasRead(bool wasRead)
{
    p->metadataWasRead = wasRead;
}

void Track::setId(int id)
{
    p->id = id;
}

void Track::setHash(const QString& hash)
{
    p->hash = hash;
}

void Track::setFilePath(const QString& path)
{
    if(path.isEmpty()) {
        return;
    }

    p->filepath = path;

    if(Track::isArchivePath(path)) {
        p->isInArchive = true;
        p->splitArchiveUrl();
    }
    else {
        p->isInArchive = false;
        p->archivePath.clear();
        p->filepathWithinArchive.clear();
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
        p->artists = {};
    }
    else {
        p->artists = internStrings(*p, StringPool::Domain::Artist, artists);
    }

    if(!p->hash.isEmpty()) {
        generateHash();
    }
}

void Track::setAlbum(const QString& title)
{
    p->album = internStringId(*p, StringPool::Domain::Album, title);

    if(!p->hash.isEmpty()) {
        generateHash();
    }
}

void Track::setAlbumArtists(const QStringList& artists)
{
    if(artists.size() == 1 && artists.front().isEmpty()) {
        p->albumArtists = {};
    }
    else {
        p->albumArtists = internStrings(*p, StringPool::Domain::AlbumArtist, artists);
    }
}

void Track::setTrackNumber(const QString& number)
{
    if(number.contains(u'/')) {
        const auto& parts = number.split(u'/', Qt::SkipEmptyParts);
        if(!parts.empty()) {
            p->trackNumber = parts.at(0);
            if(parts.size() > 1) {
                p->trackTotal = parts.at(1);
            }
        }
    }
    else {
        p->trackNumber = number;
    }

    if(!p->hash.isEmpty()) {
        generateHash();
    }
}

void Track::setTrackTotal(const QString& total)
{
    p->trackTotal = total;
}

void Track::setDiscNumber(const QString& number)
{
    if(number.contains(u'/')) {
        const auto& parts = number.split(u'/', Qt::SkipEmptyParts);
        if(!parts.empty()) {
            p->discNumber = parts.at(0);
            if(parts.size() > 1) {
                p->discTotal = parts.at(1);
            }
        }
    }
    else {
        p->discNumber = number;
    }

    if(!p->hash.isEmpty()) {
        generateHash();
    }
}

void Track::setDiscTotal(const QString& total)
{
    p->discTotal = total;
}

void Track::setGenres(const QStringList& genres)
{
    if(genres.size() == 1 && genres.front().isEmpty()) {
        p->genres = {};
    }
    else {
        p->genres = internStrings(*p, StringPool::Domain::Genre, genres);
    }
}

void Track::setComposers(const QStringList& composers)
{
    if(composers.size() == 1 && composers.front().isEmpty()) {
        p->composers = {};
    }
    else {
        p->composers = internStrings(*p, StringPool::Domain::Composer, composers);
    }
}

void Track::setPerformers(const QStringList& performers)
{
    if(performers.size() == 1 && performers.front().isEmpty()) {
        p->performers = {};
    }
    else {
        p->performers = internStrings(*p, StringPool::Domain::Performer, performers);
    }
}

void Track::setComment(const QString& comment)
{
    p->comment = comment;
}

void Track::setDate(const QString& date)
{
    p->date = date;
    if(date.isEmpty()) {
        p->year = -1;
        return;
    }

    // TODO: Replace with std::chrono::parse once full compiler support is availble

    static const QRegularExpression fullDateRegex{QLatin1String{FullDateRegex}};
    static const QRegularExpression yearMonthRegex{QLatin1String{YearMonthRegex}};
    static const QRegularExpression yearRegex{QLatin1String{YearRegex}};

    auto processDate = [this](const std::chrono::year_month_day& ymd) {
        const std::chrono::sys_days days{ymd};
        auto timePoint    = std::chrono::time_point_cast<std::chrono::milliseconds>(days);
        p->dateSinceEpoch = timePoint.time_since_epoch().count();

        const std::chrono::sys_days startOfYear{ymd.year() / 1 / 1};
        auto yearTimePoint = std::chrono::time_point_cast<std::chrono::milliseconds>(startOfYear);
        p->yearSinceEpoch  = yearTimePoint.time_since_epoch().count();
        p->year            = static_cast<int>(ymd.year());
    };

    // First try to match the full date
    QRegularExpressionMatch match = fullDateRegex.match(date);
    if(match.hasMatch()) {
        bool yearOk{false};
        bool monthOk{false};
        bool dayOk{false};
        const int year  = match.captured(1).toInt(&yearOk);
        const int month = match.captured(2).toInt(&monthOk);
        const int day   = match.captured(3).toInt(&dayOk);

        if(yearOk && monthOk && dayOk) {
            const std::chrono::year_month_day ymd{std::chrono::year{year} / month / day};
            processDate(ymd);
            return;
        }
    }

    // Then try to match the year and month
    match = yearMonthRegex.match(date);
    if(match.hasMatch()) {
        bool yearOk{false};
        bool monthOk{false};
        const int year  = match.captured(1).toInt(&yearOk);
        const int month = match.captured(2).toInt(&monthOk);

        if(yearOk && monthOk) {
            const std::chrono::year_month_day ymd{std::chrono::year{year} / month / 1};
            processDate(ymd);
            return;
        }
    }

    // Finally, try to match just the year
    match = yearRegex.match(date);
    if(match.hasMatch()) {
        bool yearOk{false};
        const int year = match.captured(0).toInt(&yearOk);

        if(yearOk) {
            const std::chrono::year_month_day ymd{std::chrono::year{year} / 1 / 1};
            processDate(ymd);
            return;
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

void Track::setRGTrackGain(float gain)
{
    p->rgTrackGain = gain;
}

void Track::setRGAlbumGain(float gain)
{
    p->rgAlbumGain = gain;
}

void Track::setRGTrackPeak(float peak)
{
    p->rgTrackPeak = peak;
}

void Track::setRGAlbumPeak(float peak)
{
    p->rgAlbumPeak = peak;
}

void Track::clearRGInfo()
{
    p->rgTrackGain = Constants::InvalidGain;
    p->rgAlbumGain = Constants::InvalidGain;
    p->rgTrackPeak = Constants::InvalidPeak;
    p->rgAlbumPeak = Constants::InvalidPeak;
}

QString Track::metaValue(const QString& name) const
{
    const QString tag = name.toUpper();

    const auto& map = metaMap();
    if(map.contains(tag)) {
        return map.at(tag)(*this);
    }

    return extraTag(tag).join(QLatin1String{Constants::UnitSeparator});
}

QString Track::techInfo(const QString& name) const
{
    auto validNum = [](auto num) -> QString {
        if(num > 0) {
            return QString::number(num);
        }
        return {};
    };

    const QString prop = name.toUpper();

    using namespace Constants::MetaData;

    // clang-format off
    static const std::unordered_map<QString, std::function<QString(const Track& track)>> infoMap{
        {QString::fromLatin1(Codec),        [](const Track& track) { return track.codec(); }},
        {QString::fromLatin1(CodecProfile), [](const Track& track) { return track.codecProfile(); }},
        {QString::fromLatin1(Tool),         [](const Track& track) { return track.tool(); }},
        {QString::fromLatin1(Encoding),     [](const Track& track) { return track.tagType(u","_s); }},
        {QString::fromLatin1(TagType),      [](const Track& track) { return track.encoding(); }},
        {QString::fromLatin1(SampleRate),   [validNum](const Track& track) { return validNum(track.sampleRate()); }},
        {QString::fromLatin1(Bitrate),      [validNum](const Track& track) { return validNum(track.bitrate()); }},
        {QString::fromLatin1(Channels),     [validNum](const Track& track) { return validNum(track.channels()); }},
        {QString::fromLatin1(BitDepth),     [validNum](const Track& track) { return validNum(track.bitDepth()); }},
        {QString::fromLatin1(Duration),     [validNum](const Track& track) { return validNum(track.duration()); }},
        {QString::fromLatin1(RGTrackGain),  [](const Track& track) { return track.hasTrackGain() ? QString::number(track.rgTrackGain()) : QString{}; }},
        {QString::fromLatin1(RGTrackPeak),  [](const Track& track) { return track.hasTrackPeak() ? QString::number(track.rgTrackPeak()) : QString{}; }},
        {QString::fromLatin1(RGAlbumGain),  [](const Track& track) { return track.hasAlbumGain() ? QString::number(track.rgAlbumGain()) : QString{}; }},
        {QString::fromLatin1(RGAlbumPeak),  [](const Track& track) { return track.hasAlbumPeak() ? QString::number(track.rgAlbumPeak()) : QString{}; }}
    };
    // clang-format on

    if(infoMap.contains(prop)) {
        return infoMap.at(prop)(*this);
    }

    return extraTag(prop).join(QLatin1String{Constants::UnitSeparator});
}

std::optional<int64_t> Track::dateValue(const QString& name) const
{
    const QString prop = name.toUpper();

    using namespace Constants::MetaData;

    // clang-format off
    static const std::unordered_map<QString, std::function<std::optional<int64_t>(const Track& track)>> dateMap{
        {QString::fromLatin1(Date),         [](const Fooyin::Track& track) { return track.p->dateSinceEpoch; }},
        {QString::fromLatin1(Year),         [](const Fooyin::Track& track) { return track.p->yearSinceEpoch; }},
        {QString::fromLatin1(FirstPlayed),  [](const Fooyin::Track& track) { return track.firstPlayed(); }},
        {QString::fromLatin1(LastPlayed),   [](const Fooyin::Track& track) { return track.lastPlayed(); }},
        {QString::fromLatin1(AddedTime),    [](const Fooyin::Track& track) { return track.addedTime(); }},
        {QString::fromLatin1(LastModified), [](const Fooyin::Track& track) { return track.lastModified(); }}
    };
    // clang-format on

    if(dateMap.contains(prop)) {
        return dateMap.at(prop)(*this);
    }

    return {};
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
    p->ensureExtraTagsLoaded();
    p->extraTags.valuesFor(internExtraTagKey(*p, tag)).push_back(value);
    p->extraTagsBlob.clear();
    p->extraTagsDirty = true;
}

void Track::addExtraTag(const QString& tag, const QStringList& value)
{
    if(tag.isEmpty() || value.isEmpty()) {
        return;
    }
    p->ensureExtraTagsLoaded();
    p->extraTags.valuesFor(internExtraTagKey(*p, tag)).append(value);
    p->extraTagsBlob.clear();
    p->extraTagsDirty = true;
}

void Track::removeExtraTag(const QString& tag)
{
    p->ensureExtraTagsLoaded();
    const QString extraTag = tag.toUpper();
    if(p->extraTags.contains(extraTag)) {
        p->removedTags.append(internExtraTagKey(*p, extraTag));
        p->extraTags.remove(extraTag);
        p->extraTagsBlob.clear();
        p->extraTagsDirty = true;
    }
}

void Track::replaceExtraTag(const QString& tag, const QString& value)
{
    p->ensureExtraTagsLoaded();
    const QString extraTag = internExtraTagKey(*p, tag);
    if(value.isEmpty()) {
        removeExtraTag(extraTag);
    }
    else {
        p->extraTags.setValues(extraTag, {value});
        p->extraTagsBlob.clear();
        p->extraTagsDirty = true;
    }
}

void Track::replaceExtraTag(const QString& tag, const QStringList& value)
{
    p->ensureExtraTagsLoaded();
    const QString extraTag = internExtraTagKey(*p, tag);

    if(value.isEmpty()) {
        removeExtraTag(extraTag);
    }
    else {
        p->extraTags.setValues(extraTag, value);
        p->extraTagsBlob.clear();
        p->extraTagsDirty = true;
    }
}

void Track::clearExtraTags()
{
    p->ensureExtraTagsLoaded();
    p->extraTags.clear();
    p->extraTagsBlob.clear();
    p->extraTagsDirty = true;
}

void Track::storeExtraTags(const QByteArray& tags)
{
    p->extraTags.clear();
    p->extraTagsBlob   = tags;
    p->extraTagsLoaded = tags.isEmpty();
    p->extraTagsDirty  = false;
}

void Track::setExtraProperty(const QString& prop, const QString& value)
{
    p->extraProps[prop] = value;
}

void Track::removeExtraProperty(const QString& prop)
{
    p->extraProps.remove(prop);
}

void Track::clearExtraProperties()
{
    p->extraProps.clear();
}

void Track::storeExtraProperties(const QByteArray& props)
{
    if(props.isEmpty()) {
        return;
    }

    QByteArray in{props};
    QDataStream stream(&in, QIODevice::ReadOnly);
    stream.setVersion(QDataStream::Qt_6_0);

    stream >> p->extraProps;
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

void Track::setCodec(const QString& codec)
{
    p->codec = internStringId(*p, StringPool::Domain::Codec, codec);
}

void Track::setCodecProfile(const QString& profile)
{
    p->codecProfile = profile;
}

void Track::setTool(const QString& tool)
{
    p->tool = tool;
}

void Track::setTagTypes(const QStringList& tagTypes)
{
    p->tagTypes = tagTypes;
}

void Track::setEncoding(const QString& encoding)
{
    p->encoding = internStringId(*p, StringPool::Domain::Encoding, encoding);
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

void Track::clearWasModified()
{
    p->metadataWasModified = false;
}

QString Track::findCommonField(const TrackList& tracks)
{
    if(tracks.size() < 2) {
        return {};
    }

    const Track& firstTrack = tracks.front();

    QString primaryArtist{firstTrack.effectiveAlbumArtist()};
    QString primaryAlbum{firstTrack.album()};
    QString primaryGenre{firstTrack.genre()};
    QString primaryDir{firstTrack.directory()};

    const auto hasSameField = [&tracks]<typename Func>(const QString& field, const Func& trackFunc) {
        return !field.isEmpty() && std::ranges::all_of(tracks, [&field, &trackFunc](const Track& track) {
            if constexpr(std::is_member_function_pointer_v<Func>) {
                return (track.*trackFunc)() == field;
            }
            else {
                return trackFunc(track) == field;
            }
        });
    };

    const bool sameArtist
        = hasSameField(primaryArtist, [](const Track& track) { return track.effectiveAlbumArtist(); });
    const bool sameAlbum = hasSameField(primaryAlbum, &Track::album);

    if(sameArtist) {
        primaryArtist.replace(QLatin1String{Constants::UnitSeparator}, ", "_L1);
    }

    if(sameAlbum) {
        if(sameArtist) {
            return u"%1 - %2"_s.arg(primaryArtist, primaryAlbum);
        }
        return primaryAlbum;
    }

    if(sameArtist) {
        return primaryArtist;
    }

    const bool sameGenre = hasSameField(primaryGenre, &Track::genre);
    if(sameGenre) {
        primaryGenre.replace(QLatin1String{Constants::UnitSeparator}, ", "_L1);
        return primaryGenre;
    }

    const bool sameDir = hasSameField(primaryDir, &Track::directory);
    if(sameDir) {
        return primaryDir;
    }

    return {};
}

TrackIds Track::trackIdsForTracks(const TrackList& tracks)
{
    TrackIds trackIds;
    for(const auto& track : tracks) {
        trackIds.emplace_back(track.id());
    }
    return trackIds;
}

QStringList Track::supportedMimeTypes()
{
    static const QStringList supportedTypes
        = {u"audio/ogg"_s,       u"audio/x-vorbis+ogg"_s, u"audio/mpeg"_s,
           u"audio/mpeg3"_s,     u"audio/x-mpeg"_s,       u"audio/x-aiff"_s,
           u"audio/x-aifc"_s,    u"audio/vnd.wave"_s,     u"audio/wav"_s,
           u"audio/x-wav"_s,     u"audio/x-musepack"_s,   u"audio/x-ape"_s,
           u"audio/x-wavpack"_s, u"audio/mp4"_s,          u"audio/vnd.audible.aax"_s,
           u"audio/flac"_s,      u"audio/ogg"_s,          u"audio/x-vorbis+ogg"_s,
           u"application/ogg"_s, u"audio/opus"_s,         u"audio/x-opus+ogg"_s,
           u"audio/x-ms-wma"_s,  u"video/x-ms-asf"_s,     u"application/vnd.ms-asf"_s};
    return supportedTypes;
}

size_t qHash(const Track& track)
{
    return qHash(track.uniqueFilepath());
}
} // namespace Fooyin
