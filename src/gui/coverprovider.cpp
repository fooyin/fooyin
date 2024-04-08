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
#include <utils/async.h>
#include <utils/crypto.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QCoro/QCoroCore>

#include <QByteArray>
#include <QFileInfo>
#include <QPixmapCache>

#include <set>
#include <stack>

constexpr QSize MaxCoverSize = {800, 800};

namespace {
QString generateCoverKey(const QString& hash, Fooyin::Track::Cover type, const QSize& size)
{
    return Fooyin::Utils::generateHash(QString::number(static_cast<int>(type)), hash, QString::number(size.width()),
                                       QString::number(size.height()));
}

QString coverThumbnailPath(const QString& key)
{
    return Fooyin::Gui::coverPath() + key + QStringLiteral(".jpg");
}

QCoro::Task<void> saveThumbnail(QPixmap cover, QString key)
{
    co_return co_await Fooyin::Utils::asyncExec([&cover, &key]() {
        QFile file{coverThumbnailPath(key)};
        file.open(QIODevice::WriteOnly);
        cover.save(&file, "JPG", 85);
    });
}

QPixmap loadNoCover(const QSize& size, Fooyin::Track::Cover type)
{
    const QString key = generateCoverKey(QStringLiteral("|NoCover|"), type, size);

    QPixmap cover;

    if(QPixmapCache::find(key, &cover)) {
        return cover;
    }

    const static QString noCoverKey = QString::fromLatin1(Fooyin::Constants::NoCover);

    if(cover.load(noCoverKey)) {
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

    SettingsManager* settings;

    bool usePlacerholder{true};
    QString coverKey;
    std::set<QString> pendingCovers;
    ScriptParser parser;

    GuiSettings::CoverPaths paths;

    explicit Private(CoverProvider* self_, SettingsManager* settings_)
        : self{self_}
        , settings{settings_}
        , paths{settings->value<Settings::Gui::Internal::TrackCoverPaths>().value<GuiSettings::CoverPaths>()}
    {
        settings->subscribe<Settings::Gui::Internal::TrackCoverPaths>(
            self, [this](const QVariant& var) { paths = var.value<GuiSettings::CoverPaths>(); });
    }

    QString findCover(const Track& track, Track::Cover type)
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

    QCoro::Task<void> fetchCover(QString key, Track track, QSize size, Track::Cover type, bool saveToDisk)
    {
        QPixmap cover;

        // Prefer artwork in directory
        const QString coverPath = co_await Utils::asyncExec([this, track, type]() { return findCover(track, type); });
        if(!coverPath.isEmpty()) {
            cover.load(coverPath);
        }

        if(cover.isNull()) {
            const QByteArray coverData
                = co_await Utils::asyncExec([track, type]() { return Tagging::readCover(track, type); });
            if(coverData.isEmpty() || !cover.loadFromData(coverData)) {
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
        emit self->coverAdded(track);
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

QPixmap CoverProvider::trackCover(const Track& track, const QSize& size, Track::Cover type, bool saveToDisk) const
{
    if(!track.isValid()) {
        return p->usePlacerholder ? loadNoCover(size, type) : QPixmap{};
    }

    const QString coverKey = !p->coverKey.isEmpty() ? p->coverKey : generateCoverKey(track.albumHash(), type, size);

    if(!p->pendingCovers.contains(coverKey)) {
        QPixmap cover = loadCachedCover(coverKey);
        if(!cover.isNull()) {
            return cover;
        }

        p->pendingCovers.emplace(coverKey);
        p->fetchCover(coverKey, track, size, type, saveToDisk);
    }

    return p->usePlacerholder ? loadNoCover(size, type) : QPixmap{};
}

QPixmap CoverProvider::trackCover(const Track& track, Track::Cover type, bool saveToDisk) const
{
    return trackCover(track, MaxCoverSize, type, saveToDisk);
}

void CoverProvider::clearCache()
{
    QDir cache{Fooyin::Gui::coverPath()};
    cache.removeRecursively();

    QPixmapCache::clear();
}

void CoverProvider::removeFromCache(const QString& key)
{
    QDir cache{Fooyin::Gui::coverPath()};
    cache.remove(coverThumbnailPath(key));

    QPixmapCache::remove(key);
}
} // namespace Fooyin

#include "gui/moc_coverprovider.cpp"
