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

#pragma once

#include "coverartworktypes.h"
#include "fygui_export.h"

#include <core/track.h>

#include <QFuture>
#include <QObject>

#include <memory>
#include <optional>
#include <set>

class QPixmap;
class QSize;

namespace Fooyin {
class AudioLoader;
class RemoteIoService;
class SettingsManager;

class CoverRepositoryPrivate;

class FYGUI_EXPORT CoverRepository : public QObject
{
    Q_OBJECT

public:
    CoverRepository(std::shared_ptr<AudioLoader> audioLoader, SettingsManager* settings, QObject* parent = nullptr);
    CoverRepository(std::shared_ptr<AudioLoader> audioLoader, std::shared_ptr<RemoteIoService> remoteIo,
                    SettingsManager* settings, QObject* parent = nullptr);
    ~CoverRepository() override;

    /** Returns @c true if @p track has a cover of the specified @p type. */
    [[nodiscard]] QFuture<bool> trackHasCover(const Track& track, Track::Cover type = Track::Cover::Front,
                                              std::optional<ArtworkSourcePreference> source = {}) const;
    /*!
     * Returns a cached cover for @p track if available.
     * If the cover is not cached, an asynchronous load is started and an empty pixmap is returned.
     */
    [[nodiscard]] QPixmap trackCover(const Track& track, Track::Cover type = Track::Cover::Front,
                                     std::optional<ArtworkSourcePreference> source = {}) const;
    /** Asynchronously returns a cover capped to the standard maximum artwork size. */
    [[nodiscard]] QFuture<QPixmap> trackCoverFull(const Track& track, Track::Cover type = Track::Cover::Front,
                                                  std::optional<ArtworkSourcePreference> source = {}) const;
    /** Asynchronously returns the original-size cover without applying the standard size cap. */
    [[nodiscard]] QFuture<QPixmap> trackCoverOriginal(const Track& track, Track::Cover type = Track::Cover::Front,
                                                      std::optional<ArtworkSourcePreference> source = {}) const;
    /** Asynchronously returns the thumbnail cover for @p track at @p size. */
    [[nodiscard]] QFuture<QPixmap> trackCoverThumbnailAsync(const Track& track, ThumbnailSize size,
                                                            Track::Cover type = Track::Cover::Front,
                                                            std::optional<ArtworkSourcePreference> source = {}) const;
    /*!
     * This is an overloaded function.
     * The requested pixel size is mapped to the nearest thumbnail bucket.
     */
    [[nodiscard]] QFuture<QPixmap> trackCoverThumbnailAsync(const Track& track, const QSize& size,
                                                            Track::Cover type = Track::Cover::Front,
                                                            std::optional<ArtworkSourcePreference> source = {}) const;
    /*!
     * Returns a cached thumbnail for @p track if available.
     * If the thumbnail is not cached, an asynchronous load is started and an empty pixmap is returned.
     */
    [[nodiscard]] QPixmap trackCoverThumbnail(const Track& track, ThumbnailSize size,
                                              Track::Cover type                             = Track::Cover::Front,
                                              std::optional<ArtworkSourcePreference> source = {}) const;
    /*!
     * This is an overloaded function.
     * The requested pixel size is mapped to the nearest thumbnail bucket.
     */
    [[nodiscard]] QPixmap trackCoverThumbnail(const Track& track, const QSize& size,
                                              Track::Cover type                             = Track::Cover::Front,
                                              std::optional<ArtworkSourcePreference> source = {}) const;

    /** Returns the placeholder cover pixmap for @p type and @p size. */
    [[nodiscard]] QPixmap placeholderCover(Track::Cover type  = Track::Cover::Front,
                                           ThumbnailSize size = ThumbnailSize::None) const;

    /*!
     * Returns the grouped artwork key used for thumbnails of @p track.
     * Tracks in the same configured thumbnail group share this key.
     */
    [[nodiscard]] QString thumbnailCoverKey(const Track& track, Track::Cover type = Track::Cover::Front,
                                            std::optional<ArtworkSourcePreference> source = {}) const;
    /*!
     * Returns the exact in-memory cache key for a thumbnail of @p track at @p size.
     * This key can be passed to setVisibleThumbnailKeys().
     */
    [[nodiscard]] QString thumbnailCacheKey(const Track& track, ThumbnailSize size,
                                            Track::Cover type                             = Track::Cover::Front,
                                            std::optional<ArtworkSourcePreference> source = {}) const;
    /*!
     * This is an overloaded function.
     * The requested pixel size is mapped to the nearest thumbnail bucket.
     */
    [[nodiscard]] QString thumbnailCacheKey(const Track& track, const QSize& size,
                                            Track::Cover type                             = Track::Cover::Front,
                                            std::optional<ArtworkSourcePreference> source = {}) const;

    /*!
     * Registers the thumbnail cache keys currently visible for @p owner.
     * Visible thumbnails are kept pinned while their normal cache entries are being reloaded or evicted.
     * Passing an empty set clears the owner's visible keys.
     */
    void setVisibleThumbnailKeys(QObject* owner, const std::set<QString>& keys);
    /** Clears all visible thumbnail keys registered by @p owner. */
    void clearVisibleThumbnailKeys(QObject* owner);

    /** Clears the in-memory cover cache and the on-disk thumbnail cache. */
    void clearCache();
    /** Removes all cached artwork for @p track using the repository settings. */
    void removeFromCache(const Track& track);
    /** Removes all cached artwork for @p track using @p settings to resolve grouped thumbnail keys. */
    void removeFromCache(const Track& track, const SettingsManager& settings);

Q_SIGNALS:
    /** Emitted after cached artwork for @p track has changed or been invalidated. */
    void coverAdded(const Fooyin::Track& track);
    /** Emitted when the configured placeholder artwork changes. */
    void placeholderChanged();

private:
    std::unique_ptr<CoverRepositoryPrivate> p;
};
} // namespace Fooyin
