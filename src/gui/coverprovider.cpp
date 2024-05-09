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

#include "core/tagging/tagreader.h"
#include "internalguisettings.h"

#include <core/scripting/scriptparser.h>
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
    return Fooyin::Utils::generateHash(QString::number(static_cast<int>(type)), track.albumHash());
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
} // namespace

namespace Fooyin {
struct CoverProvider::Private
{
    CoverProvider* self;

    SettingsManager* settings;

    bool usePlacerholder{true};
    QString coverKey;
    QPixmapCache::Key noCoverKey;
    QSize size;
    std::set<QString> pendingCovers;
    std::set<QString> noCoverKeys;
    ScriptParser parser;

    CoverPaths paths;

    std::mutex fetchGuard;

    explicit Private(CoverProvider* self_, SettingsManager* settings_)
        : self{self_}
        , settings{settings_}
        , size{settings->value<Settings::Gui::Internal::ArtworkThumbnailSize>(),
               settings->value<Settings::Gui::Internal::ArtworkThumbnailSize>()}
        , paths{settings->value<Settings::Gui::Internal::TrackCoverPaths>().value<CoverPaths>()}
    {
        settings->subscribe<Settings::Gui::Internal::ArtworkThumbnailSize>(self, [this](const int thumbSize) {
            size = {thumbSize, thumbSize};
        });
        settings->subscribe<Settings::Gui::Internal::TrackCoverPaths>(
            self, [this](const QVariant& var) { paths = var.value<CoverPaths>(); });
        settings->subscribe<Settings::Gui::IconTheme>(self, [this]() { QPixmapCache::remove(noCoverKey); });
    }

    [[nodiscard]] QPixmap loadCachedCover(const QString& key) const
    {
        QPixmap cover;

        if(QPixmapCache::find(key, &cover)) {
            return cover;
        }

        return {};
    }

    QPixmap loadNoCover()
    {
        QPixmap cachedCover;
        if(QPixmapCache::find(noCoverKey, &cachedCover)) {
            return cachedCover;
        }

        const QIcon icon = Fooyin::Utils::iconFromTheme(Fooyin::Constants::Icons::NoCover);
        static const QSize coverSize{MaxSize, MaxSize};
        const QPixmap cover = icon.pixmap(coverSize);

        noCoverKey = QPixmapCache::insert(cover);

        return cover;
    }

    QString findDirectoryCover(const Track& track, Track::Cover type)
    {
        if(!track.isValid()) {
            return {};
        }

        QStringList filters;

        if(type == Track::Cover::Front) {
            for(const auto& path : paths.frontCoverPaths) {
                filters.emplace_back(parser.evaluate(path, track));
            }
        }
        else if(type == Track::Cover::Back) {
            for(const auto& path : paths.backCoverPaths) {
                filters.emplace_back(parser.evaluate(path, track));
            }
        }
        else if(type == Track::Cover::Artist) {
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
                return filePath.absolutePath() + QStringLiteral("/") + fileList.constFirst();
            }
        }

        return {};
    }

    void fetchCover(const QString& key, const Track& track, Track::Cover type, bool thumbnail)
    {
        Utils::asyncExec([this, key, track, type, thumbnail]() {
            QImage image;

            const std::scoped_lock lock{fetchGuard};

            bool isThumb{thumbnail};
            QString coverPath;

            if(isThumb) {
                const QString cachePath = coverThumbnailPath(key);
                if(QFileInfo::exists(cachePath)) {
                    coverPath = cachePath;
                }
            }

            if(coverPath.isEmpty()) {
                coverPath = findDirectoryCover(track, type);
                if(!coverPath.isEmpty()) {
                    image.load(coverPath);
                }
            }

            if(!image.isNull()) {
                // Only store thumbnails in disk cache for embedded artwork
                isThumb = false;
            }
            else {
                const QByteArray coverData = Tagging::readCover(track, type);
                if(!coverData.isEmpty()) {
                    image.loadFromData(coverData);
                }
            }

            if(!image.isNull() && (!isThumb || !coverKey.isEmpty())) {
                image = Utils::scaleImage(image, MaxSize);
            }

            if(isThumb) {
                const QString cachePath = coverThumbnailPath(key);
                if(image.isNull()) {
                    QFile::remove(cachePath);
                }
                else if(!QFileInfo::exists(cachePath)) {
                    if(coverKey.isEmpty()) {
                        image = Utils::scaleImage(image, size);
                    }
                    saveThumbnail(image, key);
                }
            }

            return image;
        }).then(self, [this, key, track](const QImage& image) {
            if(image.isNull()) {
                return;
            }

            const QPixmap cover = QPixmap::fromImage(image);

            if(!QPixmapCache::insert(key, cover)) {
                qDebug() << "Failed to cache cover for:" << track.filepath();
            }

            pendingCovers.erase(key);
            emit self->coverAdded(track);
        });
    }
};

CoverProvider::CoverProvider(SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(this, settings)}
{ }

void CoverProvider::setUsePlaceholder(bool enabled)
{
    p->usePlacerholder = enabled;
}

void CoverProvider::setCoverKey(const QString& name)
{
    p->coverKey = name;
}

void CoverProvider::resetCoverKey()
{
    p->coverKey.clear();
}

CoverProvider::~CoverProvider() = default;

QPixmap CoverProvider::trackCover(const Track& track, Track::Cover type) const
{
    if(!track.isValid()) {
        return p->usePlacerholder ? p->loadNoCover() : QPixmap{};
    }

    QString coverKey{p->coverKey};
    if(coverKey.isEmpty()) {
        coverKey = generateCoverKey(track, type);
    }

    if(!p->pendingCovers.contains(coverKey)) {
        QPixmap cover = p->loadCachedCover(coverKey);
        if(!cover.isNull()) {
            return cover;
        }

        p->pendingCovers.emplace(coverKey);
        p->fetchCover(coverKey, track, type, !p->coverKey.isEmpty());
    }

    return p->usePlacerholder ? p->loadNoCover() : QPixmap{};
}

QPixmap CoverProvider::trackCoverThumbnail(const Track& track, Track::Cover type) const
{
    if(!track.isValid()) {
        return p->usePlacerholder ? p->loadNoCover() : QPixmap{};
    }

    QString coverKey{p->coverKey};
    if(coverKey.isEmpty()) {
        coverKey = generateCoverKey(track, type);
    }

    if(!p->pendingCovers.contains(coverKey)) {
        QPixmap cover = p->loadCachedCover(coverKey);
        if(!cover.isNull()) {
            return cover;
        }

        p->pendingCovers.emplace(coverKey);
        p->fetchCover(coverKey, track, type, true);
    }

    return p->usePlacerholder ? p->loadNoCover() : QPixmap{};
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
}
} // namespace Fooyin

#include "gui/moc_coverprovider.cpp"
