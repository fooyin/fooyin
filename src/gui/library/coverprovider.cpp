/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

#include "guipaths.h"

#include <core/tagging/tagreader.h>
#include <core/track.h>
#include <gui/guiconstants.h>
#include <utils/async.h>
#include <utils/crypto.h>
#include <utils/utils.h>

#include <QCoroCore>

#include <QByteArray>
#include <QFileInfo>
#include <QPixmapCache>

#include <set>

constexpr auto NoCoverKey    = "|NoCover|";
constexpr QSize MaxCoverSize = {800, 800};

namespace {
QString generateCoverKey(const QString& hash, const QSize& size)
{
    return Fooyin::Utils::generateHash(hash, QString::number(size.width()), QString::number(size.height()));
}

QString coverThumbnailPath(const QString& key)
{
    return Fooyin::Gui::coverPath() + key + ".jpg";
}

QCoro::Task<void> saveThumbnail(QPixmap cover, QString key)
{
    co_return co_await Fooyin::Utils::asyncExec([&cover, &key]() {
        QFile file{coverThumbnailPath(key)};
        file.open(QIODevice::WriteOnly);
        cover.save(&file, "JPG", 85);
    });
}

QPixmap loadNoCover(const QSize& size)
{
    const QString key = generateCoverKey(NoCoverKey, size);

    QPixmap cover;

    if(QPixmapCache::find(key, &cover)) {
        return cover;
    }

    if(cover.load(Fooyin::Constants::NoCover)) {
        cover = Fooyin::Utils::scalePixmap(cover, size);
        QPixmapCache::insert(key, cover);
        return cover;
    }

    return {};
}

QPixmap loadCachedCover(const QString& key)
{
    QPixmap cover;

    if(QPixmapCache::find(key, &cover)) {
        return cover;
    }

    const QString cachePath = coverThumbnailPath(key);

    if(QFileInfo::exists(cachePath)) {
        cover.load(cachePath);
        if(!cover.isNull()) {
            return cover;
        }
        QFile::remove(cachePath);
    }

    return {};
}
} // namespace

namespace Fooyin {
struct CoverProvider::Private
{
    CoverProvider* self;

    TagReader tagReader;
    std::set<QString> pendingCovers;

    explicit Private(CoverProvider* self)
        : self{self}
    { }

    QCoro::Task<void> fetchCover(QString key, Fooyin::Track track, QSize size, bool saveToDisk)
    {
        QPixmap cover;

        if(track.hasEmbeddedCover()) {
            const QByteArray coverData
                = co_await Utils::asyncExec([this, &track]() { return tagReader.readCover(track); });
            if(!cover.loadFromData(coverData)) {
                co_return;
            }
        }
        else if(QFileInfo::exists(track.coverPath())) {
            cover.load(track.coverPath());
            if(cover.isNull()) {
                co_return;
            }
        }

        cover = Utils::scalePixmap(cover, size);

        QPixmapCache::insert(key, cover);

        if(saveToDisk) {
            const QString cachePath = coverThumbnailPath(key);
            if(!QFileInfo::exists(cachePath)) {
                co_await saveThumbnail(cover, key);
            }
        }

        pendingCovers.erase(key);
        QMetaObject::invokeMethod(self, "coverAdded", Q_ARG(Fooyin::Track, track));
        co_return;
    }
};

CoverProvider::CoverProvider(QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(this)}
{ }

CoverProvider::~CoverProvider() = default;

QPixmap CoverProvider::trackCover(const Track& track, const QSize& size, bool saveToDisk) const
{
    if(!track.hasCover()) {
        return loadNoCover(size);
    }

    const QString coverKey = generateCoverKey(track.albumHash(), size);

    if(!p->pendingCovers.contains(coverKey)) {
        QPixmap cover = loadCachedCover(coverKey);
        if(!cover.isNull()) {
            return cover;
        }

        p->pendingCovers.emplace(coverKey);
        p->fetchCover(coverKey, track, size, saveToDisk);
    }

    return loadNoCover(size);
}

QPixmap CoverProvider::trackCover(const Track& track, bool saveToDisk) const
{
    return trackCover(track, MaxCoverSize, saveToDisk);
}

void CoverProvider::clearCache()
{
    QDir cache{Fooyin::Gui::coverPath()};
    cache.removeRecursively();

    QPixmapCache::clear();
}
} // namespace Fooyin

#include "gui/moc_coverprovider.cpp"
