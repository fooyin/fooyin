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

#include <core/scripting/scriptparser.h>
#include <core/tagging/tagloader.h>
#include <core/track.h>
#include <gui/guiconstants.h>
#include <gui/guipaths.h>
#include <gui/guisettings.h>
#include <utils/async.h>
#include <utils/crypto.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QByteArray>
#include <QFileInfo>
#include <QIcon>
#include <QPixmapCache>

#include <set>

constexpr auto MaxSize = 1024;

namespace {
QString generateCoverKey(const Fooyin::Track& track, Fooyin::Track::Cover type)
{
    return Fooyin::Utils::generateHash(QStringLiteral("FyCover") + QString::number(static_cast<int>(type)),
                                       track.albumHash());
}

QString generateThumbCoverKey(const QString& key)
{
    return Fooyin::Utils::generateHash(QStringLiteral("Thumb"), key);
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

QString findDirectoryCover(const Fooyin::CoverPaths& paths, const Fooyin::Track& track, Fooyin::Track::Cover type)
{
    if(!track.isValid()) {
        return {};
    }

    Fooyin::ScriptParser parser;
    QStringList filters;

    if(type == Fooyin::Track::Cover::Front) {
        for(const auto& path : paths.frontCoverPaths) {
            filters.emplace_back(parser.evaluate(path, track));
        }
    }
    else if(type == Fooyin::Track::Cover::Back) {
        for(const auto& path : paths.backCoverPaths) {
            filters.emplace_back(parser.evaluate(path, track));
        }
    }
    else if(type == Fooyin::Track::Cover::Artist) {
        for(const auto& path : paths.artistPaths) {
            filters.emplace_back(parser.evaluate(path, track));
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

struct CoverLoader
{
    QString key;
    Fooyin::Track track;
    Fooyin::Track::Cover type;
    std::shared_ptr<Fooyin::TagLoader> tagLoader;
    Fooyin::CoverPaths paths;
    QSize size;
    double dpr{1.0};
    bool storeThumb{false};
    bool isThumb{false};
    bool limitThumbSize{false};
};

QImage loadImageFromDirectory(CoverLoader& loader)
{
    const QString dirPath = findDirectoryCover(loader.paths, loader.track, loader.type);

    if(!dirPath.isEmpty()) {
        QImage image;
        image.load(dirPath);

        if(!image.isNull() && loader.isThumb && !loader.storeThumb) {
            loader.isThumb = false;
            return Fooyin::Utils::scaleImage(image, loader.size, loader.dpr);
        }
    }

    return {};
}

QImage loadImageFromTagLoader(const Fooyin::TagLoader* tagLoader, const Fooyin::Track& track, Fooyin::Track::Cover type)
{
    if(auto* tagParser = tagLoader->parserForTrack(track)) {
        const QByteArray coverData = tagParser->readCover(track, type);

        if(!coverData.isEmpty()) {
            QImage image;
            image.loadFromData(coverData);
            return image;
        }
    }

    return {};
}

void handleThumbnail(const CoverLoader& loader, QImage& image, const QString& cachePath)
{
    if(!loader.isThumb) {
        return;
    }

    if(image.isNull()) {
        QFile::remove(cachePath);
    }
    else if(!QFileInfo::exists(cachePath)) {
        if(loader.limitThumbSize) {
            image = Fooyin::Utils::scaleImage(image, loader.size, loader.dpr);
        }
        saveThumbnail(image, loader.key);
    }
}

struct CoverLoaderResult
{
    QImage cover;
    bool isThumb{false};
};

CoverLoaderResult loadCoverImage(CoverLoader loader)
{
    CoverLoaderResult result{{}, loader.isThumb};

    const QString cachePath = coverThumbnailPath(loader.key);

    // First check disk cache
    if(result.isThumb && QFileInfo::exists(cachePath)) {
        result.cover.load(cachePath);
    }

    // Then check directory paths
    if(result.cover.isNull()) {
        result.cover = loadImageFromDirectory(loader);
    }

    // Finally check metadata
    if(result.cover.isNull()) {
        result.cover = loadImageFromTagLoader(loader.tagLoader.get(), loader.track, loader.type);
    }

    if(!result.cover.isNull()) {
        result.cover = Fooyin::Utils::scaleImage(result.cover, MaxSize, loader.dpr);
    }

    handleThumbnail(loader, result.cover, cachePath);

    return result;
}
} // namespace

namespace Fooyin {
struct CoverProvider::Private
{
    CoverProvider* m_self;
    std::shared_ptr<TagLoader> m_tagLoader;
    SettingsManager* m_settings;

    double m_windowDpr{1.0};
    bool m_usePlacerholder{true};
    QString m_coverKey;
    bool m_storeThumbnail{false};
    bool m_limitThumbSize{true};
    QPixmapCache::Key m_noCoverKey;
    QSize m_size;
    std::set<QString> m_pendingCovers;
    std::set<QString> m_noCoverKeys;

    CoverPaths m_paths;

    explicit Private(CoverProvider* self, std::shared_ptr<TagLoader> tagLoader, SettingsManager* settings)
        : m_self{self}
        , m_tagLoader{std::move(tagLoader)}
        , m_settings{settings}
        , m_windowDpr{m_settings->value<Settings::Gui::MainWindowPixelRatio>()}
        , m_size{m_settings->value<Settings::Gui::Internal::ArtworkThumbnailSize>(),
                 m_settings->value<Settings::Gui::Internal::ArtworkThumbnailSize>()}
        , m_paths{m_settings->value<Settings::Gui::Internal::TrackCoverPaths>().value<CoverPaths>()}
    {
        m_settings->subscribe<Settings::Gui::Internal::ArtworkThumbnailSize>(m_self, [this](const int thumbSize) {
            m_size = {thumbSize, thumbSize};
        });
        m_settings->subscribe<Settings::Gui::Internal::TrackCoverPaths>(
            m_self, [this](const QVariant& var) { m_paths = var.value<CoverPaths>(); });
        m_settings->subscribe<Settings::Gui::IconTheme>(m_self, [this]() { QPixmapCache::remove(m_noCoverKey); });
        m_settings->subscribe<Settings::Gui::MainWindowPixelRatio>(m_self,
                                                                   [this](double ratio) { m_windowDpr = ratio; });
    }

    [[nodiscard]] static QPixmap loadCachedCover(const QString& key, bool isThumb = false)
    {
        QPixmap cover;

        if(QPixmapCache::find(isThumb ? generateThumbCoverKey(key) : key, &cover)) {
            return cover;
        }

        return {};
    }

    QPixmap loadNoCover()
    {
        QPixmap cachedCover;
        if(QPixmapCache::find(m_noCoverKey, &cachedCover)) {
            return cachedCover;
        }

        const QIcon icon = Fooyin::Utils::iconFromTheme(Fooyin::Constants::Icons::NoCover);
        static const QSize coverSize{MaxSize, MaxSize};
        const QPixmap cover = icon.pixmap(coverSize, m_windowDpr);

        m_noCoverKey = QPixmapCache::insert(cover);

        return cover;
    }

    void processCoverResult(const QString& key, const Fooyin::Track& track, const CoverLoaderResult& result)
    {
        m_pendingCovers.erase(key);

        if(result.cover.isNull()) {
            return;
        }

        const QPixmap cover = QPixmap::fromImage(result.cover);

        if(!QPixmapCache::insert(result.isThumb ? generateThumbCoverKey(key) : key, cover)) {
            qDebug() << "Failed to cache cover for:" << track.filepath();
        }

        emit m_self->coverAdded(track);
    }

    void fetchCover(const QString& key, const Track& track, Track::Cover type, bool thumbnail)
    {
        CoverLoader loader;
        loader.key            = key;
        loader.track          = track;
        loader.type           = type;
        loader.tagLoader      = m_tagLoader;
        loader.paths          = m_paths;
        loader.size           = m_size;
        loader.dpr            = m_windowDpr;
        loader.storeThumb     = m_storeThumbnail;
        loader.isThumb        = thumbnail;
        loader.limitThumbSize = m_limitThumbSize;

        auto loaderResult = Utils::asyncExec([loader]() -> CoverLoaderResult { return loadCoverImage(loader); });

        loaderResult.then(
            m_self, [this, key, track](const CoverLoaderResult& result) { processCoverResult(key, track, result); });
    }
};

CoverProvider::CoverProvider(std::shared_ptr<TagLoader> tagLoader, SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(this, std::move(tagLoader), settings)}
{ }

CoverProvider::~CoverProvider() = default;

void CoverProvider::setUsePlaceholder(bool enabled)
{
    p->m_usePlacerholder = enabled;
}

void CoverProvider::setCoverKey(const QString& name)
{
    p->m_coverKey = name;
}

void CoverProvider::resetCoverKey()
{
    p->m_coverKey.clear();
}

void CoverProvider::setLimitThumbSize(bool enabled)
{
    p->m_limitThumbSize = enabled;
}

void CoverProvider::setAlwaysStoreThumbnail(bool enabled)
{
    p->m_storeThumbnail = enabled;
}

QPixmap CoverProvider::trackCover(const Track& track, Track::Cover type) const
{
    if(!track.isValid()) {
        return p->m_usePlacerholder ? p->loadNoCover() : QPixmap{};
    }

    QString coverKey{p->m_coverKey};
    if(coverKey.isEmpty()) {
        coverKey = generateCoverKey(track, type);
    }

    if(!p->m_pendingCovers.contains(coverKey)) {
        QPixmap cover = p->loadCachedCover(coverKey, false);
        if(!cover.isNull()) {
            return cover;
        }

        p->m_pendingCovers.emplace(coverKey);
        p->fetchCover(coverKey, track, type, false);
    }

    return p->m_usePlacerholder ? p->loadNoCover() : QPixmap{};
}

QPixmap CoverProvider::trackCoverThumbnail(const Track& track, Track::Cover type) const
{
    if(!track.isValid()) {
        return p->m_usePlacerholder ? p->loadNoCover() : QPixmap{};
    }

    QString coverKey{p->m_coverKey};
    if(coverKey.isEmpty()) {
        coverKey = generateCoverKey(track, type);
    }

    if(!p->m_pendingCovers.contains(coverKey)) {
        QPixmap cover = p->loadCachedCover(coverKey, true);
        if(!cover.isNull()) {
            return cover;
        }

        p->m_pendingCovers.emplace(coverKey);
        p->fetchCover(coverKey, track, type, true);
    }

    return p->m_usePlacerholder ? p->loadNoCover() : QPixmap{};
}

void CoverProvider::clearCache()
{
    QDir cache{Fooyin::Gui::coverPath()};
    cache.removeRecursively();

    QPixmapCache::clear();
}

void CoverProvider::removeFromCache(const Track& track)
{
    removeFromCache(generateCoverKey(track, Track::Cover::Front));
    removeFromCache(generateCoverKey(track, Track::Cover::Back));
    removeFromCache(generateCoverKey(track, Track::Cover::Artist));
}

void CoverProvider::removeFromCache(const QString& key)
{
    QDir cache{Fooyin::Gui::coverPath()};
    cache.remove(coverThumbnailPath(key));

    QPixmapCache::remove(key);
    const QString thumbKey = generateThumbCoverKey(key);
    QPixmapCache::remove(thumbKey);
}
} // namespace Fooyin

#include "gui/moc_coverprovider.cpp"
