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

#include <gui/coverrepository.h>

#include "internalguisettings.h"

#include <core/engine/audioloader.h>
#include <core/network/remoteioservice.h>
#include <core/scripting/scriptparser.h>
#include <core/track.h>
#include <gui/guiconstants.h>
#include <gui/guipaths.h>
#include <gui/guisettings.h>
#include <gui/iconloader.h>
#include <utils/async.h>
#include <utils/crypto.h>
#include <utils/fileutils.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QBuffer>
#include <QByteArray>
#include <QCache>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QImageReader>
#include <QLoggingCategory>
#include <QMimeDatabase>
#include <QPromise>

#include <algorithm>
#include <chrono>
#include <iterator>
#include <map>
#include <ranges>
#include <set>
#include <tuple>
#include <unordered_map>
#include <vector>

Q_LOGGING_CATEGORY(COV_REPO, "fy.coverrepository")

using namespace Qt::StringLiterals;

constexpr auto MaxSize                 = 1024;
constexpr int64_t RetryIntervalMs      = 5000;
constexpr auto MaxPinnedThumbnailCount = 512;
constexpr auto MaxActivePixmapRequests = 3;

namespace Fooyin {
namespace {
std::vector<ThumbnailSize> placeholderSizes()
{
    return {ThumbnailSize::None,    ThumbnailSize::Tiny,  ThumbnailSize::Small,     ThumbnailSize::MediumSmall,
            ThumbnailSize::Medium,  ThumbnailSize::Large, ThumbnailSize::VeryLarge, ThumbnailSize::ExtraLarge,
            ThumbnailSize::XxLarge, ThumbnailSize::Huge,  ThumbnailSize::Full};
}

std::vector<QUrl> remoteArtworkUrls(const Track& track)
{
    static const QStringList tags{
        u"ARTWORKURL"_s,
        u"COVERART"_s,
        u"IMAGE"_s,
        u"STREAMURL"_s,
    };

    std::vector<QUrl> urls;
    std::set<QString> seenUrls;

    for(const QString& tag : tags) {
        const auto values = track.extraTag(tag);
        for(const QString& value : values) {
            const QUrl url{value.trimmed()};
            const QString scheme = url.scheme().toLower();
            if(!url.isValid() || url.isRelative() || (scheme != "http"_L1 && scheme != "https"_L1)) {
                continue;
            }
            const QString key = url.toString();
            if(seenUrls.insert(key).second) {
                urls.push_back(url);
            }
        }
    }

    return urls;
}

std::optional<QUrl> remoteArtworkUrl(const Track& track)
{
    const auto urls = remoteArtworkUrls(track);
    if(!urls.empty()) {
        return urls.front();
    }
    return {};
}

QString generateGroupedCoverKey(const QString& group, Track::Cover type, ArtworkSourcePreference sourcePreference)
{
    return Utils::generateHash(u"FyCover|%1|%2"_s.arg(static_cast<int>(type)).arg(static_cast<int>(sourcePreference)),
                               group);
}

QString generateTrackCoverKey(const Track& track, Track::Cover type, ArtworkSourcePreference sourcePreference)
{
    if(track.isRemote() && type == Track::Cover::Front) {
        if(const auto url = remoteArtworkUrl(track)) {
            return Utils::generateHash(
                u"FyRemoteCover|%1|%2"_s.arg(static_cast<int>(type)).arg(static_cast<int>(sourcePreference)),
                url->toString());
        }
    }

    return Utils::generateHash(u"FyCover|%1|%2"_s.arg(static_cast<int>(type)).arg(static_cast<int>(sourcePreference)),
                               track.hash());
}

QString generateThumbCoverKey(const QString& key, int size)
{
    return Utils::generateHash(u"Thumb|%1|%2"_s.arg(key).arg(size));
}

QString generateThumbCoverKey(const QString& key, ThumbnailSize size)
{
    return generateThumbCoverKey(key, coverThumbnailPixelSize(size));
}

QString pendingCoverKey(const QString& key, bool thumbnail, ThumbnailSize size)
{
    return thumbnail ? generateThumbCoverKey(key, size) : key;
}

QString coverThumbnailPath(const QString& key)
{
    return Gui::coverPath() + key + u".jpg"_s;
}

QString noCoverCacheKey(Track::Cover type, int size)
{
    return size > 0 ? u"|NoCover|%1|%2"_s.arg(static_cast<int>(type)).arg(size)
                    : u"|NoCover|%1|"_s.arg(static_cast<int>(type));
}

template <typename T>
QFuture<T> makeReadyFuture(T value)
{
    QPromise<T> promise;
    promise.start();
    promise.addResult(std::move(value));
    promise.finish();
    return promise.future();
}

bool saveThumbnail(const QImage& cover, const QString& key)
{
    QFile file{coverThumbnailPath(key)};
    if(file.open(QIODevice::WriteOnly)) {
        return cover.save(&file, "JPG", 85);
    }
    return false;
}

QSize calculateScaledSize(const QSize& originalSize, int maxSize)
{
    int newWidth{0};
    int newHeight{0};

    if(originalSize.width() > originalSize.height()) {
        newWidth  = maxSize;
        newHeight = (maxSize * originalSize.height()) / originalSize.width();
    }
    else {
        newHeight = maxSize;
        newWidth  = (maxSize * originalSize.width()) / originalSize.height();
    }

    return {newWidth, newHeight};
}

QString findDirectoryCover(const CoverPaths& paths, const Track& track, Track::Cover type)
{
    if(!track.isValid()) {
        return {};
    }

    static ScriptParser parser;
    static std::mutex parserGuard;
    const std::scoped_lock lock{parserGuard};

    QStringList filters;

    if(type == Track::Cover::Front) {
        for(const auto& path : paths.frontCoverPaths) {
            filters.emplace_back(parser.evaluate(path.trimmed(), track));
        }
    }
    else if(type == Track::Cover::Back) {
        for(const auto& path : paths.backCoverPaths) {
            filters.emplace_back(parser.evaluate(path.trimmed(), track));
        }
    }
    else if(type == Track::Cover::Artist) {
        for(const auto& path : paths.artistPaths) {
            filters.emplace_back(parser.evaluate(path.trimmed(), track));
        }
    }

    for(const auto& filter : filters) {
        const QStringList coverPaths = Utils::File::filesFromWildcardPath(filter);
        if(!coverPaths.empty()) {
            return coverPaths.constFirst();
        }
    }

    return {};
}

QString evaluateThumbnailGroupScript(const QString& script, const Track& track)
{
    static ScriptParser parser;
    static std::mutex parserGuard;
    const std::scoped_lock lock{parserGuard};

    return parser.evaluate(script, track);
}

QImage readImage(const QString& path, int requestedSize, const QString& hintType)
{
    const QMimeDatabase mimeDb;
    const auto mimeType   = mimeDb.mimeTypeForFile(path, QMimeDatabase::MatchContent);
    const auto formatHint = mimeType.preferredSuffix().toLocal8Bit().toLower();

    QImageReader reader{path, formatHint};

    if(!reader.canRead()) {
        qCDebug(COV_REPO) << "Failed to use format hint" << formatHint << "when trying to load" << hintType << "cover";

        reader.setFormat({});
        reader.setFileName(path);
        if(!reader.canRead()) {
            qCDebug(COV_REPO) << "Failed to load" << hintType << "cover";
            return {};
        }
    }

    const auto size    = reader.size();
    const auto maxSize = requestedSize == 0 ? MaxSize : requestedSize;
    const auto dpr     = Utils::windowDpr();

    if(size.width() > maxSize || size.height() > maxSize || dpr > 1.0) {
        const auto scaledSize = calculateScaledSize(size, static_cast<int>(maxSize * dpr));
        reader.setScaledSize(scaledSize);
    }

    QImage image = reader.read();
    image.setDevicePixelRatio(dpr);

    return image;
}

QImage readImageOriginal(const QString& path, const QString& hintType)
{
    const QMimeDatabase mimeDb;
    const auto mimeType   = mimeDb.mimeTypeForFile(path, QMimeDatabase::MatchContent);
    const auto formatHint = mimeType.preferredSuffix().toLocal8Bit().toLower();

    QImageReader reader{path, formatHint};

    if(!reader.canRead()) {
        qCDebug(COV_REPO) << "Failed to use format hint" << formatHint << "when trying to load" << hintType << "cover";

        reader.setFormat({});
        reader.setFileName(path);
        if(!reader.canRead()) {
            qCDebug(COV_REPO) << "Failed to load" << hintType << "cover";
            return {};
        }
    }

    return reader.read();
}

QImage readImage(QByteArray data)
{
    QBuffer buffer{&data};
    const QMimeDatabase mimeDb;
    const auto mimeType   = mimeDb.mimeTypeForData(&buffer);
    const auto formatHint = mimeType.preferredSuffix().toLocal8Bit().toLower();

    QImageReader reader{&buffer, formatHint};

    if(!reader.canRead()) {
        qCDebug(COV_REPO) << "Failed to use format hint" << formatHint << "when trying to load embedded cover";

        reader.setFormat({});
        reader.setDevice(&buffer);
        if(!reader.canRead()) {
            qCDebug(COV_REPO) << "Failed to load embedded cover";
            return {};
        }
    }

    const auto size = reader.size();
    if(size.width() > MaxSize || size.height() > MaxSize) {
        const auto scaledSize = calculateScaledSize(size, MaxSize);
        reader.setScaledSize(scaledSize);
    }

    return reader.read();
}

QImage readImageOriginal(QByteArray data)
{
    QBuffer buffer{&data};
    const QMimeDatabase mimeDb;
    const auto mimeType   = mimeDb.mimeTypeForData(&buffer);
    const auto formatHint = mimeType.preferredSuffix().toLocal8Bit().toLower();

    QImageReader reader{&buffer, formatHint};

    if(!reader.canRead()) {
        qCDebug(COV_REPO) << "Failed to use format hint" << formatHint << "when trying to load embedded cover";

        reader.setFormat({});
        reader.setDevice(&buffer);
        if(!reader.canRead()) {
            qCDebug(COV_REPO) << "Failed to load embedded cover";
            return {};
        }
    }

    return reader.read();
}

struct CoverLoader
{
    QString key;
    Track track;
    Track::Cover type;
    ArtworkSourcePreference sourcePreference{ArtworkSourcePreference::PreferDirectory};
    std::shared_ptr<AudioLoader> audioLoader;
    CoverPaths paths;
    bool isThumb{false};
    ThumbnailSize size{ThumbnailSize::None};
    bool originalSize{false};
    QImage cover;
};

bool prefersEmbedded(const CoverLoader& loader)
{
    return loader.sourcePreference == ArtworkSourcePreference::PreferEmbedded;
}

bool hasImageInDirectory(const CoverLoader& loader)
{
    const QString dirPath = findDirectoryCover(loader.paths, loader.track, loader.type);
    if(dirPath.isEmpty()) {
        return {};
    }

    const QFile file{dirPath};
    return file.size() > 0;
}

QImage loadImageFromDirectory(const CoverLoader& loader)
{
    const QString dirPath = findDirectoryCover(loader.paths, loader.track, loader.type);
    if(dirPath.isEmpty()) {
        return {};
    }

    const QFile file{dirPath};
    if(file.size() == 0) {
        return {};
    }

    return loader.originalSize ? readImageOriginal(dirPath, u"directory"_s)
                               : readImage(dirPath, coverThumbnailPixelSize(loader.size), u"directory"_s);
}

bool hasEmbeddedCover(const CoverLoader& loader)
{
    const QByteArray coverData = loader.audioLoader->readTrackCover(loader.track, loader.type);
    return !coverData.isEmpty();
}

bool hasRemoteArtwork(const CoverLoader& loader)
{
    return loader.type == Track::Cover::Front && loader.track.isRemote() && !remoteArtworkUrls(loader.track).empty();
}

QImage loadImageFromEmbedded(const CoverLoader& loader, const QString& cachePath)
{
    const QByteArray coverData = loader.audioLoader->readTrackCover(loader.track, loader.type);
    if(coverData.isEmpty()) {
        return {};
    }

    QImage cover = loader.originalSize ? readImageOriginal(coverData) : readImage(coverData);

    if(loader.isThumb && !cover.isNull() && !QFileInfo::exists(cachePath)) {
        if(!saveThumbnail(cover, loader.key)) {
            qCInfo(COV_REPO) << "Failed to save cover thumbnail for track:" << loader.track.filepath();
        }
        cover = Fooyin::Utils::scaleImage(cover, coverThumbnailPixelSize(loader.size), Fooyin::Utils::windowDpr());
    }

    return cover;
}

QImage loadImageFromRemoteArtworkData(const CoverLoader& loader, const QString& cachePath, const QByteArray& coverData)
{
    if(loader.type != Track::Cover::Front) {
        return {};
    }

    if(coverData.isEmpty()) {
        return {};
    }

    QImage cover = loader.originalSize ? readImageOriginal(coverData) : readImage(coverData);
    if(cover.isNull()) {
        return {};
    }

    if(loader.isThumb && !QFileInfo::exists(cachePath)) {
        if(!saveThumbnail(cover, loader.key)) {
            qCInfo(COV_REPO) << "Failed to save remote cover thumbnail for track:" << loader.track.filepath();
        }
        cover = Utils::scaleImage(cover, coverThumbnailPixelSize(loader.size), Utils::windowDpr());
    }

    return cover;
}

bool hasCoverImage(const CoverLoader& loader)
{
    if(prefersEmbedded(loader)) {
        return hasRemoteArtwork(loader) || hasEmbeddedCover(loader) || hasImageInDirectory(loader);
    }

    return hasRemoteArtwork(loader) || hasImageInDirectory(loader) || hasEmbeddedCover(loader);
}

CoverLoader loadCoverImage(const CoverLoader& loader)
{
    CoverLoader result{loader};

    const QString cachePath = coverThumbnailPath(loader.key);

    // First check disk cache
    if(result.isThumb && QFileInfo::exists(cachePath)) {
        result.cover = readImage(cachePath, coverThumbnailPixelSize(loader.size), u"cached"_s);
    }

    if(prefersEmbedded(loader)) {
        if(result.cover.isNull()) {
            result.cover = loadImageFromEmbedded(loader, cachePath);
        }
        if(result.cover.isNull()) {
            result.cover = loadImageFromDirectory(loader);
        }
    }
    else {
        if(result.cover.isNull()) {
            result.cover = loadImageFromDirectory(loader);
        }
        if(result.cover.isNull()) {
            result.cover = loadImageFromEmbedded(loader, cachePath);
        }
    }

    return result;
}

struct PinnedThumbnail
{
    QPixmap cover;
    uint64_t age{0};
};

enum class CoverRequestKind : uint8_t
{
    Full = 0,
    Original,
    Thumbnail
};

struct CoverRequestKey
{
    QString key;
    CoverRequestKind kind;
    ThumbnailSize size{ThumbnailSize::None};

    [[nodiscard]] bool operator==(const CoverRequestKey& other) const = default;
    [[nodiscard]] bool operator<(const CoverRequestKey& other) const
    {
        return std::tie(key, kind, size) < std::tie(other.key, other.kind, other.size);
    }
};

struct PendingPixmapRequest
{
    std::vector<QPromise<QPixmap>> promises;
    std::vector<std::shared_ptr<RemoteDownloadHandle>> downloads;
    bool notifyOnFinished{false};
};

struct QueuedPixmapRequest
{
    CoverRequestKey key;
    CoverLoader loader;
    int priority{0};
    uint64_t sequence{0};
};
} // namespace

class FYGUI_NO_EXPORT CoverRepositoryPrivate
{
public:
    explicit CoverRepositoryPrivate(CoverRepository* self, std::shared_ptr<AudioLoader> audioLoader,
                                    std::shared_ptr<RemoteIoService> remoteIo, SettingsManager* settings);

    QPixmap loadNoCover(Track::Cover type = Track::Cover::Front, ThumbnailSize size = ThumbnailSize::None);
    [[nodiscard]] ArtworkSourcePreference sourcePreference(std::optional<ArtworkSourcePreference> source) const;
    QPixmap processLoadResult(const CoverLoader& loader);
    static QPixmap processOriginalLoadResult(const CoverLoader& loader);
    void cachePixmap(const QString& key, const QPixmap& cover, ThumbnailSize size = ThumbnailSize::None);
    QFuture<QPixmap> requestPixmap(const CoverRequestKey& requestKey, const CoverLoader& loader,
                                   bool notifyOnFinished = false);
    void finishPixmapRequest(const CoverRequestKey& requestKey, const CoverLoader& result);
    void finishPixmapRequestFromLocalLoad(const CoverRequestKey& requestKey, const CoverLoader& loader);
    void requestRemoteArtwork(const CoverRequestKey& requestKey, const CoverLoader& loader, std::vector<QUrl> urls,
                              size_t index = 0);
    void processPixmapQueue();
    void reprioritiseQueuedThumbnails();
    void cancelPendingPixmapRequest(const CoverRequestKey& requestKey);
    void finishAndClearPendingPixmapRequests();
    [[nodiscard]] int requestPriority(const CoverRequestKey& requestKey, bool notifyOnFinished) const;
    QFuture<QPixmap> loadCover(const QString& key, const Track& track, Track::Cover type,
                               ArtworkSourcePreference sourcePreference, bool notifyOnFinished = false);
    QFuture<QPixmap> loadOriginalCover(const QString& key, const Track& track, Track::Cover type,
                                       ArtworkSourcePreference sourcePreference);
    QFuture<QPixmap> loadThumbnail(const QString& key, const Track& track, ThumbnailSize size, Track::Cover type,
                                   ArtworkSourcePreference sourcePreference, bool notifyOnFinished = false);
    QPixmap loadCachedCover(const QString& key, ThumbnailSize size = ThumbnailSize::None);
    QPixmap pinnedCover(const QString& key);
    [[nodiscard]] QFuture<bool> hasCover(const QString& key, const Track& track, Track::Cover type,
                                         ArtworkSourcePreference sourcePreference) const;
    [[nodiscard]] QString thumbnailCoverKey(const Track& track, Track::Cover type,
                                            ArtworkSourcePreference sourcePreference) const;
    bool shouldRetryNoCover(const QString& key);
    void clearNoCoverRetry(const QString& key);
    void clearPlaceholderCache();
    void pinLoadedThumbnail(const QString& key, const QPixmap& cover);
    void touchPinnedThumbnail(std::map<QString, PinnedThumbnail>::iterator thumbnail);
    void evictLeastRecentlyUsedPinnedThumbnail();
    void rebuildPinnedThumbnails();

    CoverRepository* m_self;
    std::shared_ptr<AudioLoader> m_audioLoader;
    std::shared_ptr<RemoteIoService> m_remoteIo;
    SettingsManager* m_settings;

    std::map<CoverRequestKey, PendingPixmapRequest> m_pendingPixmapRequests;
    std::vector<QueuedPixmapRequest> m_queuedPixmapRequests;
    int m_activePixmapRequests{0};
    uint64_t m_nextPixmapRequestSequence{0};
    std::map<QString, int64_t> m_noCoverRetryAfterMs;

    CoverPaths m_paths;
    ArtworkSourcePreference m_sourcePreference;
    QString m_thumbnailGroupScript;
    QCache<QString, QPixmap> m_coverCache;
    std::set<QString> m_noCoverKeys;
    std::unordered_map<QObject*, std::set<QString>> m_visibleThumbnailKeys;
    std::map<QString, PinnedThumbnail> m_pinnedThumbnails;
    uint64_t m_pinnedThumbnailAge{0};
};

CoverRepositoryPrivate::CoverRepositoryPrivate(CoverRepository* self, std::shared_ptr<AudioLoader> audioLoader,
                                               std::shared_ptr<RemoteIoService> remoteIo, SettingsManager* settings)
    : m_self{self}
    , m_audioLoader{std::move(audioLoader)}
    , m_remoteIo{std::move(remoteIo)}
    , m_settings{settings}
    , m_paths{m_settings->value<Settings::Gui::Internal::TrackCoverPaths>().value<CoverPaths>()}
    , m_sourcePreference{static_cast<ArtworkSourcePreference>(
          m_settings->value<Settings::Gui::Internal::TrackCoverSourcePreference>())}
    , m_thumbnailGroupScript{m_settings->value<Settings::Gui::Internal::TrackCoverThumbnailGroupScript>()}
{
    auto updateCache = [this](const int sizeMb) {
        m_coverCache.setMaxCost(sizeMb * 1024LL * 1024LL);
    };

    updateCache(m_settings->value<Settings::Gui::Internal::PixmapCacheSize>());
    m_settings->subscribe<Settings::Gui::Internal::PixmapCacheSize>(m_self, updateCache);

    m_settings->subscribe<Settings::Gui::Internal::TrackCoverPaths>(m_self, [this](const QVariant& var) {
        m_paths = var.value<CoverPaths>();
        clearPlaceholderCache();
        Q_EMIT m_self->placeholderChanged();
    });
    m_settings->subscribe<Settings::Gui::Internal::TrackCoverSourcePreference>(
        m_self, [this](int preference) { m_sourcePreference = static_cast<ArtworkSourcePreference>(preference); });
    m_settings->subscribe<Settings::Gui::Internal::TrackCoverThumbnailGroupScript>(m_self, [this](QString script) {
        m_thumbnailGroupScript = std::move(script);
        m_noCoverKeys.clear();
    });
    m_settings->subscribe<Settings::Gui::IconTheme>(m_self, [this]() {
        clearPlaceholderCache();
        Q_EMIT m_self->placeholderChanged();
    });
}

ArtworkSourcePreference CoverRepositoryPrivate::sourcePreference(std::optional<ArtworkSourcePreference> source) const
{
    return source.value_or(m_sourcePreference);
}

QString CoverRepositoryPrivate::thumbnailCoverKey(const Track& track, Track::Cover type,
                                                  ArtworkSourcePreference sourcePreference) const
{
    if(track.isRemote() && type == Track::Cover::Front) {
        if(const auto url = remoteArtworkUrl(track)) {
            return Utils::generateHash(
                u"FyRemoteCover|%1|%2"_s.arg(static_cast<int>(type)).arg(static_cast<int>(sourcePreference)),
                url->toString());
        }
    }

    QString group = evaluateThumbnailGroupScript(m_thumbnailGroupScript, track);
    if(group.isEmpty()) {
        group = track.albumHash();
    }

    return generateGroupedCoverKey(group, type, sourcePreference);
}

QPixmap CoverRepositoryPrivate::loadNoCover(Track::Cover type, const ThumbnailSize size)
{
    const int coverSize    = size == ThumbnailSize::None ? MaxSize : coverThumbnailPixelSize(size);
    const QString cacheKey = noCoverCacheKey(type, coverSize);
    if(auto* cover = m_coverCache.object(cacheKey)) {
        return *cover;
    }

    QString placeholder;
    if(type == Track::Cover::Back) {
        placeholder = m_paths.backPlaceholder;
    }
    else if(type == Track::Cover::Artist) {
        placeholder = m_paths.artistPlaceholder;
    }
    else {
        placeholder = m_paths.frontPlaceholder;
    }

    if(!placeholder.isEmpty()) {
        if(const QImage image = readImage(placeholder, coverSize, u"placeholder"_s); !image.isNull()) {
            QPixmap cover = QPixmap::fromImage(image);
            cover.setDevicePixelRatio(Utils::windowDpr());
            cachePixmap(cacheKey, cover);
            return cover;
        }
    }

    const QIcon icon = Gui::iconFromTheme(Constants::Icons::NoCover);
    const QSize requestedSize{coverSize, coverSize};

    auto* cover    = new QPixmap(icon.pixmap(requestedSize, Utils::windowDpr()));
    const int cost = cover->width() * cover->height() * cover->depth() / 8;

    if(m_coverCache.insert(cacheKey, cover, cost)) {
        return *cover;
    }

    return {};
}

void CoverRepositoryPrivate::clearPlaceholderCache()
{
    for(const auto type : {Track::Cover::Front, Track::Cover::Back, Track::Cover::Artist}) {
        for(const auto size : placeholderSizes()) {
            const int coverSize = size == ThumbnailSize::None ? MaxSize : coverThumbnailPixelSize(size);
            m_coverCache.remove(noCoverCacheKey(type, coverSize));
        }
    }
}

QPixmap CoverRepositoryPrivate::processLoadResult(const CoverLoader& loader)
{
    if(loader.cover.isNull()) {
        return {};
    }

    QPixmap cover = QPixmap::fromImage(loader.cover);
    cover.setDevicePixelRatio(Utils::windowDpr());

    if(loader.isThumb) {
        pinLoadedThumbnail(pendingCoverKey(loader.key, loader.isThumb, loader.size), cover);
    }

    return cover;
}

QPixmap CoverRepositoryPrivate::processOriginalLoadResult(const CoverLoader& loader)
{
    if(loader.cover.isNull()) {
        return {};
    }

    return QPixmap::fromImage(loader.cover);
}

void CoverRepositoryPrivate::cachePixmap(const QString& key, const QPixmap& cover, ThumbnailSize size)
{
    if(cover.isNull()) {
        return;
    }

    auto* cachedCover      = new QPixmap(cover);
    const int cost         = cachedCover->width() * cachedCover->height() * cachedCover->depth() / 8;
    const QString cacheKey = size == ThumbnailSize::None ? key : generateThumbCoverKey(key, size);

    if(!m_coverCache.insert(cacheKey, cachedCover, cost)) {
        qCDebug(COV_REPO) << "Failed to cache cover for key:" << cacheKey;
    }
}

QFuture<QPixmap> CoverRepositoryPrivate::requestPixmap(const CoverRequestKey& requestKey, const CoverLoader& loader,
                                                       bool notifyOnFinished)
{
    const int priority = requestPriority(requestKey, notifyOnFinished);

    if(const auto it = m_pendingPixmapRequests.find(requestKey); it != m_pendingPixmapRequests.end()) {
        it->second.notifyOnFinished = it->second.notifyOnFinished || notifyOnFinished;
        auto& promise               = it->second.promises.emplace_back();
        promise.start();

        for(auto& request : m_queuedPixmapRequests) {
            if(request.key == requestKey) {
                request.priority = std::min(request.priority, priority);
                break;
            }
        }

        return promise.future();
    }

    PendingPixmapRequest pendingRequest;
    pendingRequest.notifyOnFinished = notifyOnFinished;

    auto& promise = pendingRequest.promises.emplace_back();
    promise.start();
    const QFuture<QPixmap> future = promise.future();

    m_pendingPixmapRequests.emplace(requestKey, std::move(pendingRequest));
    m_queuedPixmapRequests.emplace_back(QueuedPixmapRequest{
        .key = requestKey, .loader = loader, .priority = priority, .sequence = m_nextPixmapRequestSequence++});
    processPixmapQueue();

    return future;
}

void CoverRepositoryPrivate::finishPixmapRequest(const CoverRequestKey& requestKey, const CoverLoader& result)
{
    --m_activePixmapRequests;

    const auto pendingIt = m_pendingPixmapRequests.find(requestKey);
    if(pendingIt == m_pendingPixmapRequests.cend()) {
        processPixmapQueue();
        return;
    }

    std::vector<QPromise<QPixmap>> promises = std::move(pendingIt->second.promises);
    const bool notify                       = pendingIt->second.notifyOnFinished;
    m_pendingPixmapRequests.erase(pendingIt);

    QPixmap cover;
    if(requestKey.kind == CoverRequestKind::Original) {
        cover = processOriginalLoadResult(result);
    }
    else {
        cover = processLoadResult(result);
        if(!cover.isNull()) {
            clearNoCoverRetry(result.key);
            m_noCoverKeys.erase(result.key);
            cachePixmap(result.key, cover,
                        requestKey.kind == CoverRequestKind::Thumbnail ? requestKey.size : ThumbnailSize::None);
            if(notify) {
                Q_EMIT m_self->coverAdded(result.track);
            }
        }
        else {
            m_noCoverKeys.emplace(result.key);
        }
    }

    for(auto& resultPromise : promises) {
        resultPromise.addResult(cover);
        resultPromise.finish();
    }

    processPixmapQueue();
}

void CoverRepositoryPrivate::finishPixmapRequestFromLocalLoad(const CoverRequestKey& requestKey,
                                                              const CoverLoader& loader)
{
    auto loaderResult = Utils::asyncExec([loader]() -> CoverLoader {
        auto result = loadCoverImage(loader);
        return result;
    });
    loaderResult.then(m_self,
                      [this, requestKey](const CoverLoader& result) { finishPixmapRequest(requestKey, result); });
}

void CoverRepositoryPrivate::requestRemoteArtwork(const CoverRequestKey& requestKey, const CoverLoader& loader,
                                                  std::vector<QUrl> urls, size_t index)
{
    if(index >= urls.size()) {
        finishPixmapRequestFromLocalLoad(requestKey, loader);
        return;
    }

    const auto pendingIt = m_pendingPixmapRequests.find(requestKey);
    if(pendingIt == m_pendingPixmapRequests.end()) {
        finishPixmapRequest(requestKey, {});
        return;
    }

    const QUrl url = urls.at(index);

    auto handle = m_remoteIo->download(
        url, m_self,
        [this, requestKey, loader, urls = std::move(urls), index](std::optional<QByteArray> data,
                                                                  const QString& error) mutable {
            if(!m_pendingPixmapRequests.contains(requestKey)) {
                finishPixmapRequest(requestKey, {});
                return;
            }

            if(data) {
                CoverLoader result{loader};
                result.cover = loadImageFromRemoteArtworkData(loader, coverThumbnailPath(loader.key), data.value());
                if(!result.cover.isNull()) {
                    finishPixmapRequest(requestKey, result);
                    return;
                }
            }
            else if(!error.isEmpty()) {
                qCDebug(COV_REPO) << "Could not download remote cover artwork:" << error;
            }

            requestRemoteArtwork(requestKey, loader, std::move(urls), index + 1);
        },
        std::chrono::seconds{10});

    pendingIt->second.downloads.emplace_back(std::move(handle));
}

void CoverRepositoryPrivate::processPixmapQueue()
{
    while(m_activePixmapRequests < MaxActivePixmapRequests && !m_queuedPixmapRequests.empty()) {
        const auto nextRequestIt
            = std::ranges::min_element(m_queuedPixmapRequests, [](const auto& lhs, const auto& rhs) {
                  return std::tie(lhs.priority, lhs.sequence) < std::tie(rhs.priority, rhs.sequence);
              });

        const QueuedPixmapRequest request = std::move(*nextRequestIt);
        m_queuedPixmapRequests.erase(nextRequestIt);

        if(!m_pendingPixmapRequests.contains(request.key)) {
            continue;
        }

        ++m_activePixmapRequests;

        const bool hasCachedThumbnail
            = request.loader.isThumb && QFileInfo::exists(coverThumbnailPath(request.loader.key));
        if(m_remoteIo && hasRemoteArtwork(request.loader) && !hasCachedThumbnail) {
            requestRemoteArtwork(request.key, request.loader, remoteArtworkUrls(request.loader.track));
        }
        else {
            finishPixmapRequestFromLocalLoad(request.key, request.loader);
        }
    }
}

void CoverRepositoryPrivate::reprioritiseQueuedThumbnails()
{
    std::set<QString> visibleKeys;
    for(const auto& ownerKeys : m_visibleThumbnailKeys | std::views::values) {
        visibleKeys.insert(ownerKeys.cbegin(), ownerKeys.cend());
    }

    for(auto& request : m_queuedPixmapRequests) {
        if(request.key.kind != CoverRequestKind::Thumbnail) {
            continue;
        }

        const QString thumbnailKey = generateThumbCoverKey(request.key.key, request.key.size);
        request.priority           = visibleKeys.contains(thumbnailKey) ? 0 : requestPriority(request.key, false);
    }
}

void CoverRepositoryPrivate::cancelPendingPixmapRequest(const CoverRequestKey& requestKey)
{
    std::erase_if(m_queuedPixmapRequests,
                  [&requestKey](const QueuedPixmapRequest& request) { return request.key == requestKey; });

    const auto pendingIt = m_pendingPixmapRequests.find(requestKey);
    if(pendingIt == m_pendingPixmapRequests.cend()) {
        return;
    }

    for(auto& promise : pendingIt->second.promises) {
        promise.addResult(QPixmap{});
        promise.finish();
    }
    for(const auto& download : pendingIt->second.downloads) {
        download->cancel();
    }

    m_pendingPixmapRequests.erase(pendingIt);
}

void CoverRepositoryPrivate::finishAndClearPendingPixmapRequests()
{
    m_queuedPixmapRequests.clear();

    for(auto& request : m_pendingPixmapRequests | std::views::values) {
        for(const auto& download : request.downloads) {
            download->cancel();
        }
        for(auto& promise : request.promises) {
            promise.addResult(QPixmap{});
            promise.finish();
        }
    }

    m_pendingPixmapRequests.clear();
}

int CoverRepositoryPrivate::requestPriority(const CoverRequestKey& requestKey, bool notifyOnFinished) const
{
    if(requestKey.kind == CoverRequestKind::Thumbnail && notifyOnFinished) {
        return 0;
    }
    if(requestKey.kind == CoverRequestKind::Full) {
        return 1;
    }
    if(requestKey.kind == CoverRequestKind::Thumbnail) {
        return 2;
    }
    return 3;
}

QFuture<QPixmap> CoverRepositoryPrivate::loadCover(const QString& key, const Track& track, Track::Cover type,
                                                   ArtworkSourcePreference sourcePreference, bool notifyOnFinished)
{
    if(const QPixmap cover = loadCachedCover(key); !cover.isNull()) {
        return makeReadyFuture(cover);
    }

    CoverLoader loader;
    loader.key              = key;
    loader.track            = track;
    loader.type             = type;
    loader.sourcePreference = sourcePreference;
    loader.audioLoader      = m_audioLoader;
    loader.paths            = m_paths;
    return requestPixmap(CoverRequestKey{.key = key, .kind = CoverRequestKind::Full}, loader, notifyOnFinished);
}

QFuture<QPixmap> CoverRepositoryPrivate::loadOriginalCover(const QString& key, const Track& track, Track::Cover type,
                                                           ArtworkSourcePreference sourcePreference)
{
    CoverLoader loader;
    loader.key              = key;
    loader.track            = track;
    loader.type             = type;
    loader.sourcePreference = sourcePreference;
    loader.audioLoader      = m_audioLoader;
    loader.paths            = m_paths;
    loader.originalSize     = true;

    return requestPixmap(CoverRequestKey{.key = key, .kind = CoverRequestKind::Original}, loader);
}

QFuture<QPixmap> CoverRepositoryPrivate::loadThumbnail(const QString& key, const Track& track, ThumbnailSize size,
                                                       Track::Cover type, ArtworkSourcePreference sourcePreference,
                                                       bool notifyOnFinished)
{
    CoverLoader loader;
    loader.key              = key;
    loader.track            = track;
    loader.type             = type;
    loader.sourcePreference = sourcePreference;
    loader.audioLoader      = m_audioLoader;
    loader.paths            = m_paths;
    loader.isThumb          = true;
    loader.size             = size;

    return requestPixmap(CoverRequestKey{.key = key, .kind = CoverRequestKind::Thumbnail, .size = size}, loader,
                         notifyOnFinished);
}

QPixmap CoverRepositoryPrivate::loadCachedCover(const QString& key, ThumbnailSize size)
{
    const QString cacheKey = size == ThumbnailSize::None ? key : generateThumbCoverKey(key, size);
    if(auto* cover = m_coverCache.object(cacheKey)) {
        return *cover;
    }

    return {};
}

QPixmap CoverRepositoryPrivate::pinnedCover(const QString& key)
{
    auto it = m_pinnedThumbnails.find(key);
    if(it != m_pinnedThumbnails.end() && !it->second.cover.isNull()) {
        touchPinnedThumbnail(it);
        return it->second.cover;
    }
    return {};
}

QFuture<bool> CoverRepositoryPrivate::hasCover(const QString& key, const Track& track, Track::Cover type,
                                               ArtworkSourcePreference sourcePreference) const
{
    CoverLoader loader;
    loader.key              = key;
    loader.track            = track;
    loader.type             = type;
    loader.sourcePreference = sourcePreference;
    loader.audioLoader      = m_audioLoader;
    loader.paths            = m_paths;

    auto loaderResult = Utils::asyncExec([loader]() -> bool {
        const bool result = hasCoverImage(loader);
        return result;
    });

    return loaderResult;
}

bool CoverRepositoryPrivate::shouldRetryNoCover(const QString& key)
{
    const auto now = static_cast<int64_t>(QDateTime::currentMSecsSinceEpoch());
    if(const auto it = m_noCoverRetryAfterMs.find(key); it != m_noCoverRetryAfterMs.cend() && now < it->second) {
        return false;
    }

    m_noCoverRetryAfterMs.emplace(key, now + RetryIntervalMs);
    return true;
}

void CoverRepositoryPrivate::clearNoCoverRetry(const QString& key)
{
    m_noCoverRetryAfterMs.erase(key);
}

void CoverRepositoryPrivate::pinLoadedThumbnail(const QString& key, const QPixmap& cover)
{
    if(cover.isNull()) {
        return;
    }

    bool visible{false};
    for(const auto& [owner, keys] : m_visibleThumbnailKeys) {
        if(keys.contains(key)) {
            visible = true;
            break;
        }
    }

    if(!visible) {
        return;
    }

    if(m_pinnedThumbnails.size() >= MaxPinnedThumbnailCount && !m_pinnedThumbnails.contains(key)) {
        evictLeastRecentlyUsedPinnedThumbnail();
    }

    auto result = m_pinnedThumbnails.insert_or_assign(key, PinnedThumbnail{.cover = cover});
    touchPinnedThumbnail(result.first);
}

void CoverRepositoryPrivate::touchPinnedThumbnail(std::map<QString, PinnedThumbnail>::iterator thumbnail)
{
    thumbnail->second.age = ++m_pinnedThumbnailAge;
}

void CoverRepositoryPrivate::evictLeastRecentlyUsedPinnedThumbnail()
{
    if(m_pinnedThumbnails.empty()) {
        return;
    }

    auto oldest = m_pinnedThumbnails.begin();
    for(auto it = std::next(m_pinnedThumbnails.begin()); it != m_pinnedThumbnails.end(); ++it) {
        if(it->second.age < oldest->second.age) {
            oldest = it;
        }
    }

    m_pinnedThumbnails.erase(oldest);
}

void CoverRepositoryPrivate::rebuildPinnedThumbnails()
{
    std::set<QString> visibleKeys;
    for(const auto& [owner, ownerKeys] : m_visibleThumbnailKeys) {
        visibleKeys.insert(ownerKeys.cbegin(), ownerKeys.cend());
    }

    for(auto it = m_pinnedThumbnails.begin(); it != m_pinnedThumbnails.end();) {
        if(!visibleKeys.contains(it->first)) {
            it = m_pinnedThumbnails.erase(it);
        }
        else {
            ++it;
        }
    }

    for(const QString& key : visibleKeys) {
        if(m_pinnedThumbnails.contains(key)) {
            continue;
        }
        if(auto* cover = m_coverCache.object(key)) {
            if(m_pinnedThumbnails.size() >= MaxPinnedThumbnailCount) {
                evictLeastRecentlyUsedPinnedThumbnail();
            }
            auto result = m_pinnedThumbnails.emplace(key, PinnedThumbnail{.cover = *cover});
            touchPinnedThumbnail(result.first);
        }
    }
}

CoverRepository::CoverRepository(std::shared_ptr<AudioLoader> audioLoader, SettingsManager* settings, QObject* parent)
    : CoverRepository{std::move(audioLoader), nullptr, settings, parent}
{ }

CoverRepository::CoverRepository(std::shared_ptr<AudioLoader> audioLoader, std::shared_ptr<RemoteIoService> remoteIo,
                                 SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<CoverRepositoryPrivate>(this, std::move(audioLoader), std::move(remoteIo), settings)}
{ }

CoverRepository::~CoverRepository() = default;

QFuture<bool> CoverRepository::trackHasCover(const Track& track, Track::Cover type,
                                             std::optional<ArtworkSourcePreference> source) const
{
    if(!track.isValid()) {
        return Utils::asyncExec([] { return false; });
    }

    const auto sourcePreference = p->sourcePreference(source);
    const QString coverKey      = generateTrackCoverKey(track, type, sourcePreference);

    if(p->m_noCoverKeys.contains(coverKey)) {
        return Utils::asyncExec([] { return false; });
    }

    return p->hasCover(coverKey, track, type, sourcePreference);
}

QPixmap CoverRepository::trackCover(const Track& track, Track::Cover type,
                                    std::optional<ArtworkSourcePreference> source) const
{
    if(!track.isValid()) {
        return {};
    }

    const auto sourcePreference = p->sourcePreference(source);
    const QString coverKey      = generateTrackCoverKey(track, type, sourcePreference);
    QPixmap cover               = p->loadCachedCover(coverKey);
    if(!cover.isNull()) {
        return cover;
    }

    p->loadCover(coverKey, track, type, sourcePreference, true);
    return {};
}

QFuture<QPixmap> CoverRepository::trackCoverFull(const Track& track, Track::Cover type,
                                                 std::optional<ArtworkSourcePreference> source) const
{
    if(!track.isValid()) {
        return Utils::asyncExec([] { return QPixmap{}; });
    }

    const auto sourcePreference = p->sourcePreference(source);
    const QString coverKey      = generateTrackCoverKey(track, type, sourcePreference);
    return p->loadCover(coverKey, track, type, sourcePreference);
}

QFuture<QPixmap> CoverRepository::trackCoverOriginal(const Track& track, Track::Cover type,
                                                     std::optional<ArtworkSourcePreference> source) const
{
    if(!track.isValid()) {
        return Utils::asyncExec([] { return QPixmap{}; });
    }

    const auto sourcePreference = p->sourcePreference(source);
    const QString coverKey      = generateTrackCoverKey(track, type, sourcePreference);
    return p->loadOriginalCover(coverKey, track, type, sourcePreference);
}

QFuture<QPixmap> CoverRepository::trackCoverThumbnailAsync(const Track& track, ThumbnailSize size, Track::Cover type,
                                                           std::optional<ArtworkSourcePreference> source) const
{
    if(!track.isValid()) {
        return makeReadyFuture(QPixmap{});
    }

    const auto sourcePreference = p->sourcePreference(source);
    const QString coverKey      = p->thumbnailCoverKey(track, type, sourcePreference);
    if(p->m_noCoverKeys.contains(coverKey)) {
        if(!p->shouldRetryNoCover(coverKey)) {
            return makeReadyFuture(QPixmap{});
        }

        p->m_noCoverKeys.erase(coverKey);
    }

    if(const QPixmap cover = p->loadCachedCover(coverKey, size); !cover.isNull()) {
        return makeReadyFuture(cover);
    }

    return p->loadThumbnail(coverKey, track, size, type, sourcePreference);
}

QFuture<QPixmap> CoverRepository::trackCoverThumbnailAsync(const Track& track, const QSize& size, Track::Cover type,
                                                           std::optional<ArtworkSourcePreference> source) const
{
    return trackCoverThumbnailAsync(track, coverThumbnailSizeFor(size), type, source);
}

QPixmap CoverRepository::trackCoverThumbnail(const Track& track, ThumbnailSize size, Track::Cover type,
                                             std::optional<ArtworkSourcePreference> source) const
{
    if(!track.isValid()) {
        return {};
    }

    const auto sourcePreference = p->sourcePreference(source);
    const QString coverKey      = p->thumbnailCoverKey(track, type, sourcePreference);
    const QString requestKey    = pendingCoverKey(coverKey, true, size);

    if(p->m_noCoverKeys.contains(coverKey)) {
        if(p->shouldRetryNoCover(coverKey)) {
            p->m_noCoverKeys.erase(coverKey);
            p->loadThumbnail(coverKey, track, size, type, sourcePreference, true);
        }

        return {};
    }

    QPixmap cover = p->loadCachedCover(coverKey, size);
    if(!cover.isNull()) {
        p->pinLoadedThumbnail(requestKey, cover);
        return cover;
    }

    if(const QPixmap pinnedCover = p->pinnedCover(requestKey); !pinnedCover.isNull()) {
        return pinnedCover;
    }

    p->loadThumbnail(coverKey, track, size, type, sourcePreference, true);
    return {};
}

QPixmap CoverRepository::trackCoverThumbnail(const Track& track, const QSize& size, Track::Cover type,
                                             std::optional<ArtworkSourcePreference> source) const
{
    return trackCoverThumbnail(track, coverThumbnailSizeFor(size), type, source);
}

QPixmap CoverRepository::placeholderCover(Track::Cover type, ThumbnailSize size) const
{
    return p->loadNoCover(type, size);
}

QString CoverRepository::thumbnailCoverKey(const Track& track, Track::Cover type,
                                           std::optional<ArtworkSourcePreference> source) const
{
    if(!track.isValid()) {
        return {};
    }
    return p->thumbnailCoverKey(track, type, p->sourcePreference(source));
}

QString CoverRepository::thumbnailCacheKey(const Track& track, ThumbnailSize size, Track::Cover type,
                                           std::optional<ArtworkSourcePreference> source) const
{
    const QString coverKey = thumbnailCoverKey(track, type, source);
    return coverKey.isEmpty() ? QString{} : generateThumbCoverKey(coverKey, size);
}

QString CoverRepository::thumbnailCacheKey(const Track& track, const QSize& size, Track::Cover type,
                                           std::optional<ArtworkSourcePreference> source) const
{
    return thumbnailCacheKey(track, coverThumbnailSizeFor(size), type, source);
}

void CoverRepository::setVisibleThumbnailKeys(QObject* owner, const std::set<QString>& keys)
{
    if(!owner) {
        return;
    }

    if(keys.empty()) {
        p->m_visibleThumbnailKeys.erase(owner);
    }
    else {
        const bool knownOwner = p->m_visibleThumbnailKeys.contains(owner);
        p->m_visibleThumbnailKeys.insert_or_assign(owner, keys);
        if(!knownOwner) {
            QObject::connect(owner, &QObject::destroyed, this, [this, owner]() {
                p->m_visibleThumbnailKeys.erase(owner);
                p->rebuildPinnedThumbnails();
            });
        }
    }

    p->rebuildPinnedThumbnails();
    p->reprioritiseQueuedThumbnails();
}

void CoverRepository::clearVisibleThumbnailKeys(QObject* owner)
{
    if(owner) {
        p->m_visibleThumbnailKeys.erase(owner);
        p->rebuildPinnedThumbnails();
        p->reprioritiseQueuedThumbnails();
    }
}

void CoverRepository::clearCache()
{
    QDir cache{Gui::coverPath()};
    cache.removeRecursively();

    p->m_coverCache.clear();
    p->m_noCoverKeys.clear();
    p->m_noCoverRetryAfterMs.clear();
    p->m_pinnedThumbnails.clear();
    p->m_pinnedThumbnailAge = 0;
    p->finishAndClearPendingPixmapRequests();
}

void CoverRepository::removeFromCache(const Track& track, const SettingsManager& settings)
{
    const auto thumbnailGroupScript = settings.value<Settings::Gui::Internal::TrackCoverThumbnailGroupScript>();
    QString group                   = evaluateThumbnailGroupScript(thumbnailGroupScript, track);
    if(group.isEmpty()) {
        group = track.albumHash();
    }

    auto removeKey = [this](const QString& key) {
        QDir cache{Gui::coverPath()};
        cache.remove(coverThumbnailPath(key));
        p->m_noCoverKeys.erase(key);
        p->m_noCoverRetryAfterMs.erase(key);
        p->m_coverCache.remove(key);
        p->m_pinnedThumbnails.erase(key);
        p->cancelPendingPixmapRequest(CoverRequestKey{.key = key, .kind = CoverRequestKind::Full});
        p->cancelPendingPixmapRequest(CoverRequestKey{.key = key, .kind = CoverRequestKind::Original});

        for(const auto size : {ThumbnailSize::Tiny, ThumbnailSize::Small, ThumbnailSize::MediumSmall,
                               ThumbnailSize::Medium, ThumbnailSize::Large, ThumbnailSize::VeryLarge,
                               ThumbnailSize::ExtraLarge, ThumbnailSize::XxLarge, ThumbnailSize::Huge}) {
            const QString thumbKey = generateThumbCoverKey(key, size);
            p->m_coverCache.remove(thumbKey);
            p->m_pinnedThumbnails.erase(thumbKey);
            p->cancelPendingPixmapRequest(
                CoverRequestKey{.key = key, .kind = CoverRequestKind::Thumbnail, .size = size});
        }
    };

    for(const auto type : {Track::Cover::Front, Track::Cover::Back, Track::Cover::Artist}) {
        for(const auto sourcePreference :
            {ArtworkSourcePreference::PreferDirectory, ArtworkSourcePreference::PreferEmbedded}) {
            removeKey(generateGroupedCoverKey(group, type, sourcePreference));
            removeKey(generateTrackCoverKey(track, type, sourcePreference));
        }
    }

    Q_EMIT coverAdded(track);
}

void CoverRepository::removeFromCache(const Track& track)
{
    removeFromCache(track, *p->m_settings);
}
} // namespace Fooyin

#include "gui/moc_coverrepository.cpp"
