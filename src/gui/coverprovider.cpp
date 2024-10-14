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

#include <gui/coverprovider.h>

#include "internalguisettings.h"

#include <core/engine/audioloader.h>
#include <core/scripting/scriptparser.h>
#include <core/track.h>
#include <gui/guiconstants.h>
#include <gui/guipaths.h>
#include <gui/guisettings.h>
#include <utils/async.h>
#include <utils/crypto.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QBuffer>
#include <QByteArray>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QImageReader>
#include <QLoggingCategory>
#include <QMimeDatabase>
#include <QPixmapCache>

#include <set>

Q_LOGGING_CATEGORY(COV_PROV, "fy.coverprovider")

constexpr auto MaxSize = 1024;

// Used to keep track of tracks without artwork so we don't query the filesystem more than necessary
std::set<QString> Fooyin::CoverProvider::m_noCoverKeys;

namespace {
using Fooyin::CoverProvider;

QString generateCoverKey(const Fooyin::Track& track, Fooyin::Track::Cover type)
{
    return Fooyin::Utils::generateHash(QStringLiteral("FyCover") + QString::number(static_cast<int>(type)),
                                       track.albumHash());
}

QString generateThumbCoverKey(const QString& key, int size)
{
    return Fooyin::Utils::generateHash(QStringLiteral("Thumb|%1|%2").arg(key).arg(size));
}

QString coverThumbnailPath(const QString& key)
{
    return Fooyin::Gui::coverPath() + key + QStringLiteral(".jpg");
}

void saveThumbnail(const QImage& cover, const QString& key)
{
    QFile file{coverThumbnailPath(key)};
    file.open(QIODevice::WriteOnly);
    cover.save(&file, "JPG", 85);
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

QPixmap loadCachedCover(const QString& key, int size = 0)
{
    QPixmap cover;

    if(QPixmapCache::find(size == 0 ? key : generateThumbCoverKey(key, size), &cover)) {
        return cover;
    }

    return {};
}

QString findDirectoryCover(const Fooyin::CoverPaths& paths, const Fooyin::Track& track, Fooyin::Track::Cover type)
{
    if(!track.isValid()) {
        return {};
    }

    static Fooyin::ScriptParser parser;
    static std::mutex parserGuard;
    const std::scoped_lock lock{parserGuard};

    QStringList filters;

    if(type == Fooyin::Track::Cover::Front) {
        for(const auto& path : paths.frontCoverPaths) {
            filters.emplace_back(parser.evaluate(path.trimmed(), track));
        }
    }
    else if(type == Fooyin::Track::Cover::Back) {
        for(const auto& path : paths.backCoverPaths) {
            filters.emplace_back(parser.evaluate(path.trimmed(), track));
        }
    }
    else if(type == Fooyin::Track::Cover::Artist) {
        for(const auto& path : paths.artistPaths) {
            filters.emplace_back(parser.evaluate(path.trimmed(), track));
        }
    }

    for(const auto& filter : filters) {
        const QFileInfo fileInfo{QDir::cleanPath(filter)};
        const QDir filePath{fileInfo.path()};
        const QString filePattern  = fileInfo.fileName();
        const QStringList fileList = filePath.entryList({filePattern}, QDir::Files);

        if(!fileList.isEmpty()) {
            return filePath.absoluteFilePath(fileList.constFirst());
        }
    }

    return {};
}

QImage readImage(const QString& path, int requestedSize, const QString& hintType)
{
    const QMimeDatabase mimeDb;
    const auto mimeType   = mimeDb.mimeTypeForFile(path, QMimeDatabase::MatchContent);
    const auto formatHint = mimeType.preferredSuffix().toLocal8Bit().toLower();

    QImageReader reader{path, formatHint};

    if(!reader.canRead()) {
        qCDebug(COV_PROV) << "Failed to use format hint" << formatHint << "when trying to load" << hintType << "cover";

        reader.setFormat({});
        reader.setFileName(path);
        if(!reader.canRead()) {
            qCDebug(COV_PROV) << "Failed to load" << hintType << "cover";
            return {};
        }
    }

    const auto size    = reader.size();
    const auto maxSize = requestedSize == 0 ? MaxSize : requestedSize;
    const auto dpr     = Fooyin::Utils::windowDpr();

    if(size.width() > maxSize || size.height() > maxSize || dpr > 1.0) {
        const auto scaledSize = calculateScaledSize(size, static_cast<int>(maxSize * dpr));
        reader.setScaledSize(scaledSize);
    }

    QImage image = reader.read();
    image.setDevicePixelRatio(dpr);

    return image;
}

QImage readImage(QByteArray data)
{
    QBuffer buffer{&data};
    const QMimeDatabase mimeDb;
    const auto mimeType   = mimeDb.mimeTypeForData(&buffer);
    const auto formatHint = mimeType.preferredSuffix().toLocal8Bit().toLower();

    QImageReader reader{&buffer, formatHint};

    if(!reader.canRead()) {
        qCDebug(COV_PROV) << "Failed to use format hint" << formatHint << "when trying to load embedded cover";

        reader.setFormat({});
        reader.setDevice(&buffer);
        if(!reader.canRead()) {
            qCDebug(COV_PROV) << "Failed to load embedded cover";
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

struct CoverLoader
{
    QString key;
    Fooyin::Track track;
    Fooyin::Track::Cover type;
    std::shared_ptr<Fooyin::AudioLoader> audioLoader;
    Fooyin::CoverPaths paths;
    bool isThumb{false};
    CoverProvider::ThumbnailSize size{CoverProvider::None};
    QImage cover;
};

QImage loadImageFromDirectory(CoverLoader& loader)
{
    const QString dirPath = findDirectoryCover(loader.paths, loader.track, loader.type);
    if(dirPath.isEmpty()) {
        return {};
    }

    const QFile file{dirPath};
    if(file.size() == 0) {
        return {};
    }

    return readImage(dirPath, loader.size, QStringLiteral("directory"));
}

QImage loadImageFromEmbedded(const CoverLoader& loader, const QString& cachePath)
{
    const QByteArray coverData = loader.audioLoader->readTrackCover(loader.track, loader.type);
    if(coverData.isEmpty()) {
        return {};
    }

    QImage cover = readImage(coverData);

    if(loader.isThumb && !cover.isNull() && !QFileInfo::exists(cachePath)) {
        saveThumbnail(cover, loader.key);
        cover = Fooyin::Utils::scaleImage(cover, loader.size, Fooyin::Utils::windowDpr());
    }

    return cover;
}

CoverLoader loadCoverImage(CoverLoader loader)
{
    CoverLoader result{loader};

    const QString cachePath = coverThumbnailPath(loader.key);

    // First check disk cache
    if(result.isThumb && QFileInfo::exists(cachePath)) {
        result.cover = readImage(cachePath, loader.size, QStringLiteral("cached"));
    }

    // Then check directory paths
    if(result.cover.isNull()) {
        result.cover = loadImageFromDirectory(loader);
    }

    // Finally check metadata
    if(result.cover.isNull()) {
        result.cover = loadImageFromEmbedded(loader, cachePath);
    }

    return result;
}
} // namespace

namespace Fooyin {
class FYGUI_NO_EXPORT CoverProvider::CoverProviderPrivate
{
public:
    explicit CoverProviderPrivate(CoverProvider* self, std::shared_ptr<AudioLoader> audioLoader,
                                  SettingsManager* settings);

    QPixmap loadNoCover();
    void processCoverResult(const CoverLoader& loader);
    void fetchCover(const QString& key, const Track& track, Track::Cover type, bool thumbnail,
                    CoverProvider::ThumbnailSize size = CoverProvider::None);

    CoverProvider* m_self;
    std::shared_ptr<AudioLoader> m_audioLoader;
    SettingsManager* m_settings;

    bool m_usePlacerholder{true};
    QPixmapCache::Key m_noCoverKey;
    std::set<QString> m_pendingCovers;

    CoverPaths m_paths;
};

CoverProvider::CoverProviderPrivate::CoverProviderPrivate(CoverProvider* self, std::shared_ptr<AudioLoader> audioLoader,
                                                          SettingsManager* settings)
    : m_self{self}
    , m_audioLoader{std::move(audioLoader)}
    , m_settings{settings}
    , m_paths{m_settings->value<Settings::Gui::Internal::TrackCoverPaths>().value<CoverPaths>()}
{
    m_settings->subscribe<Settings::Gui::Internal::TrackCoverPaths>(
        m_self, [this](const QVariant& var) { m_paths = var.value<CoverPaths>(); });
    m_settings->subscribe<Settings::Gui::IconTheme>(m_self, [this]() { QPixmapCache::remove(m_noCoverKey); });
}

QPixmap CoverProvider::CoverProviderPrivate::loadNoCover()
{
    QPixmap cachedCover;
    if(QPixmapCache::find(m_noCoverKey, &cachedCover)) {
        return cachedCover;
    }

    const QIcon icon = Fooyin::Utils::iconFromTheme(Fooyin::Constants::Icons::NoCover);
    static const QSize coverSize{MaxSize, MaxSize};
    const QPixmap cover = icon.pixmap(coverSize, Utils::windowDpr());

    m_noCoverKey = QPixmapCache::insert(cover);

    return cover;
}

void CoverProvider::CoverProviderPrivate::processCoverResult(const CoverLoader& loader)
{
    m_pendingCovers.erase(loader.key);

    if(loader.cover.isNull()) {
        CoverProvider::m_noCoverKeys.emplace(loader.key);
        return;
    }

    QPixmap cover = QPixmap::fromImage(loader.cover);
    cover.setDevicePixelRatio(Utils::windowDpr());

    if(!QPixmapCache::insert(loader.isThumb ? generateThumbCoverKey(loader.key, loader.size) : loader.key, cover)) {
        qCDebug(COV_PROV) << "Failed to cache cover for:" << loader.track.filepath();
    }

    emit m_self->coverAdded(loader.track);
}

void CoverProvider::CoverProviderPrivate::fetchCover(const QString& key, const Track& track, Track::Cover type,
                                                     bool thumbnail, CoverProvider::ThumbnailSize size)
{
    CoverLoader loader;
    loader.key         = key;
    loader.track       = track;
    loader.type        = type;
    loader.audioLoader = m_audioLoader;
    loader.paths       = m_paths;
    loader.isThumb     = thumbnail;
    loader.size        = size;

    auto loaderResult = Utils::asyncExec([loader]() -> CoverLoader {
        auto result = loadCoverImage(loader);
        // Make sure we destroy instance before thread quits
        loader.audioLoader->destroyThreadInstance();
        return result;
    });
    loaderResult.then(m_self, [this, key, track](const CoverLoader& result) { processCoverResult(result); });
}

CoverProvider::CoverProvider(std::shared_ptr<AudioLoader> audioLoader, SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<CoverProviderPrivate>(this, std::move(audioLoader), settings)}
{ }

CoverProvider::~CoverProvider() = default;

void CoverProvider::setUsePlaceholder(bool enabled)
{
    p->m_usePlacerholder = enabled;
}

QPixmap CoverProvider::trackCover(const Track& track, Track::Cover type) const
{
    if(!track.isValid()) {
        return p->m_usePlacerholder ? p->loadNoCover() : QPixmap{};
    }

    const QString coverKey = generateCoverKey(track, type);
    if(!p->m_pendingCovers.contains(coverKey) && !m_noCoverKeys.contains(coverKey)) {
        QPixmap cover = loadCachedCover(coverKey);
        if(!cover.isNull()) {
            return cover;
        }

        p->m_pendingCovers.emplace(coverKey);
        p->fetchCover(coverKey, track, type, false);
    }

    return p->m_usePlacerholder ? p->loadNoCover() : QPixmap{};
}

QPixmap CoverProvider::trackCoverThumbnail(const Track& track, ThumbnailSize size, Track::Cover type) const
{
    if(!track.isValid()) {
        return p->m_usePlacerholder ? p->loadNoCover() : QPixmap{};
    }

    const QString coverKey = generateCoverKey(track, type);
    if(!p->m_pendingCovers.contains(coverKey) && !m_noCoverKeys.contains(coverKey)) {
        QPixmap cover = loadCachedCover(coverKey, size);
        if(!cover.isNull()) {
            return cover;
        }

        p->m_pendingCovers.emplace(coverKey);
        p->fetchCover(coverKey, track, type, true, size);
    }

    return p->m_usePlacerholder ? p->loadNoCover() : QPixmap{};
}

QPixmap CoverProvider::trackCoverThumbnail(const Track& track, const QSize& size, Track::Cover type) const
{
    return trackCoverThumbnail(track, findThumbnailSize(size), type);
}

CoverProvider::ThumbnailSize CoverProvider::findThumbnailSize(const QSize& size)
{
    const int maxSize = std::max(size.width(), size.height());

    if(maxSize <= 32) {
        return Tiny;
    }
    if(maxSize <= 64) {
        return Small;
    }
    if(maxSize <= 96) {
        return MediumSmall;
    }
    if(maxSize <= 128) {
        return Medium;
    }
    if(maxSize <= 192) {
        return Large;
    }
    if(maxSize <= 256) {
        return VeryLarge;
    }
    if(maxSize <= 512) {
        return ExtraLarge;
    }

    return Huge;
}

void CoverProvider::clearCache()
{
    QDir cache{Fooyin::Gui::coverPath()};
    cache.removeRecursively();

    QPixmapCache::clear();
}

void CoverProvider::removeFromCache(const Track& track)
{
    auto removeKey = [](const QString& key) {
        QDir cache{Fooyin::Gui::coverPath()};
        cache.remove(coverThumbnailPath(key));
        m_noCoverKeys.erase(key);
        QPixmapCache::remove(key);
    };

    removeKey(generateCoverKey(track, Track::Cover::Front));
    removeKey(generateCoverKey(track, Track::Cover::Back));
    removeKey(generateCoverKey(track, Track::Cover::Artist));

    for(const auto type : {Track::Cover::Front, Track::Cover::Back, Track::Cover::Artist}) {
        for(const auto size : {Tiny, Small, MediumSmall, Medium, Large, VeryLarge, ExtraLarge, Huge}) {
            QPixmapCache::remove(generateThumbCoverKey(generateCoverKey(track, type), size));
        }
    }
}
} // namespace Fooyin

#include "gui/moc_coverprovider.cpp"
