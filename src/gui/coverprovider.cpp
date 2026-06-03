/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include <gui/coverrepository.h>

#include <QPixmap>

namespace Fooyin {
class FYGUI_NO_EXPORT CoverProviderPrivate
{
public:
    explicit CoverProviderPrivate(CoverRepository* repository)
        : m_repository{repository}
    { }

    CoverRepository* m_repository;
    std::unique_ptr<CoverRepository> m_ownedRepository;
    bool m_usePlaceholder{true};
    std::optional<ArtworkSourcePreference> m_sourcePreference;
};

CoverProvider::CoverProvider(std::shared_ptr<AudioLoader> audioLoader, SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<CoverProviderPrivate>(nullptr)}
{
    p->m_ownedRepository = std::make_unique<CoverRepository>(std::move(audioLoader), settings, this);
    p->m_repository      = p->m_ownedRepository.get();

    QObject::connect(p->m_repository, &CoverRepository::coverAdded, this, &CoverProvider::coverAdded);
    QObject::connect(p->m_repository, &CoverRepository::placeholderChanged, this, &CoverProvider::placeholderChanged);
}

CoverProvider::CoverProvider(CoverRepository* repository, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<CoverProviderPrivate>(repository)}
{
    QObject::connect(p->m_repository, &CoverRepository::coverAdded, this, &CoverProvider::coverAdded);
    QObject::connect(p->m_repository, &CoverRepository::placeholderChanged, this, &CoverProvider::placeholderChanged);
}

CoverProvider::~CoverProvider() = default;

void CoverProvider::setUsePlaceholder(bool enabled)
{
    p->m_usePlaceholder = enabled;
}

void CoverProvider::setSourcePreference(std::optional<ArtworkSourcePreference> preference)
{
    p->m_sourcePreference = preference;
}

QFuture<bool> CoverProvider::trackHasCover(const Track& track, Track::Cover type) const
{
    return p->m_repository->trackHasCover(track, type, p->m_sourcePreference);
}

QPixmap CoverProvider::trackCover(const Track& track, Track::Cover type) const
{
    QPixmap cover = p->m_repository->trackCover(track, type, p->m_sourcePreference);
    if(!cover.isNull()) {
        return cover;
    }

    return p->m_usePlaceholder ? p->m_repository->placeholderCover(type) : QPixmap{};
}

QFuture<QPixmap> CoverProvider::trackCoverFull(const Track& track, Track::Cover type) const
{
    return p->m_repository->trackCoverFull(track, type, p->m_sourcePreference);
}

QFuture<QPixmap> CoverProvider::trackCoverOriginal(const Track& track, Track::Cover type) const
{
    return p->m_repository->trackCoverOriginal(track, type, p->m_sourcePreference);
}

QFuture<QPixmap> CoverProvider::trackCoverThumbnailAsync(const Track& track, ThumbnailSize size,
                                                         Track::Cover type) const
{
    return p->m_repository->trackCoverThumbnailAsync(track, size, type, p->m_sourcePreference);
}

QFuture<QPixmap> CoverProvider::trackCoverThumbnailAsync(const Track& track, const QSize& size, Track::Cover type) const
{
    return p->m_repository->trackCoverThumbnailAsync(track, size, type, p->m_sourcePreference);
}

QPixmap CoverProvider::trackCoverThumbnail(const Track& track, ThumbnailSize size, Track::Cover type) const
{
    QPixmap cover = p->m_repository->trackCoverThumbnail(track, size, type, p->m_sourcePreference);
    if(!cover.isNull()) {
        return cover;
    }

    return p->m_usePlaceholder ? p->m_repository->placeholderCover(type, size) : QPixmap{};
}

QPixmap CoverProvider::trackCoverThumbnail(const Track& track, const QSize& size, Track::Cover type) const
{
    return trackCoverThumbnail(track, findThumbnailSize(size), type);
}

QPixmap CoverProvider::placeholderCover(Track::Cover type) const
{
    return p->m_repository->placeholderCover(type);
}

CoverRepository* CoverProvider::repository() const
{
    return p->m_repository;
}

QString CoverProvider::thumbnailCoverKey(const Track& track, Track::Cover type) const
{
    return p->m_repository->thumbnailCoverKey(track, type, p->m_sourcePreference);
}

QString CoverProvider::thumbnailCacheKey(const Track& track, ThumbnailSize size, Track::Cover type) const
{
    return p->m_repository->thumbnailCacheKey(track, size, type, p->m_sourcePreference);
}

QString CoverProvider::thumbnailCacheKey(const Track& track, const QSize& size, Track::Cover type) const
{
    return p->m_repository->thumbnailCacheKey(track, size, type, p->m_sourcePreference);
}

void CoverProvider::setVisibleThumbnailKeys(QObject* owner, const std::set<QString>& keys)
{
    p->m_repository->setVisibleThumbnailKeys(owner, keys);
}

void CoverProvider::clearVisibleThumbnailKeys(QObject* owner)
{
    p->m_repository->clearVisibleThumbnailKeys(owner);
}

ThumbnailSize CoverProvider::findThumbnailSize(const QSize& size)
{
    return coverThumbnailSizeFor(size);
}

void CoverProvider::clearCache()
{
    p->m_repository->clearCache();
}

void CoverProvider::removeFromCache(const Track& track)
{
    p->m_repository->removeFromCache(track);
}

void CoverProvider::removeFromCache(const Track& track, const SettingsManager& settings)
{
    p->m_repository->removeFromCache(track, settings);
}
} // namespace Fooyin

#include "gui/moc_coverprovider.cpp"
