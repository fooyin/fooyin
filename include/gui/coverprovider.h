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

#pragma once

#include "fygui_export.h"

#include <core/track.h>
#include <gui/coverartworktypes.h>

#include <QFuture>
#include <QObject>

#include <memory>
#include <optional>
#include <set>

class QPixmap;
class QString;
class QSize;

namespace Fooyin {
class AudioLoader;
class CoverRepository;
class SettingsManager;

class CoverProviderPrivate;

/*!
 * Provides access to track album artwork.
 */
class FYGUI_EXPORT CoverProvider : public QObject
{
    Q_OBJECT

public:
    explicit CoverProvider(std::shared_ptr<AudioLoader> audioLoader, SettingsManager* settings,
                           QObject* parent = nullptr);
    explicit CoverProvider(CoverRepository* repository, QObject* parent = nullptr);
    ~CoverProvider() override;

    /*!
     * If @c true, @fn trackCover methods will return a placeholder if tracks don't have covers or
     * are still being loaded.
     * @note this is enabled by default.
     */
    void setUsePlaceholder(bool enabled);

    /*!
     * Sets the local source preference for artwork.
     * If std::nullopt is passed, the global user-facing setting will be used.
     * @note the global setting is used by default.
     */
    void setSourcePreference(std::optional<ArtworkSourcePreference> preference);

    /** Returns @c true if @p track has a cover of the specific @p type. */
    [[nodiscard]] QFuture<bool> trackHasCover(const Track& track, Track::Cover type = Track::Cover::Front) const;

    /*!
     * This will return the picture of @p type for the @p track if it exists in the cache.
     * If not, it will return a QPixmap representing 'no cover', and the coverAdded signal
     * will be emitted with the same track once it has been loaded. It is then up to the caller
     * to query the cover using this method again.
     *
     * @param track the track for which the cover will be found.
     * @param type the type of cover to find.
     * @returns the cover if already in the cache or the 'no cover' cover if not.
     */
    [[nodiscard]] QPixmap trackCover(const Track& track, Track::Cover type = Track::Cover::Front) const;

    /** Returns a valid pixmap if @p track has a cover of the specific @p type. */
    [[nodiscard]] QFuture<QPixmap> trackCoverFull(const Track& track, Track::Cover type = Track::Cover::Front) const;
    /** Returns the original-size cover pixmap for @p track without applying the standard 1024px cap. */
    [[nodiscard]] QFuture<QPixmap> trackCoverOriginal(const Track& track,
                                                      Track::Cover type = Track::Cover::Front) const;
    /** Returns the thumbnail cover pixmap for @p track asynchronously. */
    [[nodiscard]] QFuture<QPixmap> trackCoverThumbnailAsync(const Track& track, ThumbnailSize size,
                                                            Track::Cover type = Track::Cover::Front) const;
    /** Returns the thumbnail cover pixmap for @p track asynchronously. */
    [[nodiscard]] QFuture<QPixmap> trackCoverThumbnailAsync(const Track& track, const QSize& size,
                                                            Track::Cover type = Track::Cover::Front) const;

    /*!
     * This will return the thumbnail picture of @p type for the @p track if it exists in the cache.
     * If not, it will return a QPixmap representing 'no cover', and the coverAdded signal
     * will be emitted with the same track once it has been loaded. It is then up to the caller
     * to query the cover using this method again.
     *
     * @param track the track for which the cover will be found.
     * @param type the type of cover to find.
     * @param size the size of the cover to return.
     * @returns the cover if already in the cache or on disk, the 'no cover' cover if not.
     */
    [[nodiscard]] QPixmap trackCoverThumbnail(const Track& track, ThumbnailSize size,
                                              Track::Cover type = Track::Cover::Front) const;
    /*!
     * This is an overloaded function.
     *
     * @param track the track for which the cover will be found.
     * @param type the type of cover to find.
     * @param size the size of the cover to return.
     * @returns the cover if already in the cache or on disk, the 'no cover' cover if not.
     */
    [[nodiscard]] QPixmap trackCoverThumbnail(const Track& track, const QSize& size,
                                              Track::Cover type = Track::Cover::Front) const;

    // Returns the placeholder cover used if a track doesn't have any artwork
    [[nodiscard]] QPixmap placeholderCover(Track::Cover type = Track::Cover::Front) const;
    /** Returns the shared repository used by this provider. */
    [[nodiscard]] CoverRepository* repository() const;

    /*!
     * Returns the grouped artwork key used for thumbnails of @p track.
     * Tracks in the same configured thumbnail group share this key.
     */
    [[nodiscard]] QString thumbnailCoverKey(const Track& track, Track::Cover type = Track::Cover::Front) const;
    /*!
     * Returns the exact in-memory cache key for a thumbnail of @p track at @p size.
     * This is intended for views that need to pin visible thumbnails.
     */
    [[nodiscard]] QString thumbnailCacheKey(const Track& track, ThumbnailSize size,
                                            Track::Cover type = Track::Cover::Front) const;
    /*!
     * This is an overloaded function.
     * The requested pixel size is mapped to the nearest thumbnail bucket.
     */
    [[nodiscard]] QString thumbnailCacheKey(const Track& track, const QSize& size,
                                            Track::Cover type = Track::Cover::Front) const;

    /*!
     * Registers the thumbnail cache keys currently visible for @p owner.
     * Visible thumbnails are kept pinned while their normal cache entries are being reloaded or evicted.
     * Passing an empty set clears the owner's visible keys.
     */
    void setVisibleThumbnailKeys(QObject* owner, const std::set<QString>& keys);
    /** Clears all visible thumbnail keys registered by @p owner. */
    void clearVisibleThumbnailKeys(QObject* owner);

    /** Returns an equivalent thumbnail size for the given @p size */
    static ThumbnailSize findThumbnailSize(const QSize& size);
    /** Clears the QPixmapCache as well as the on-disk cache. */
    void clearCache();
    /** Removes all covers of the @p track from the cache. */
    void removeFromCache(const Track& track);
    /** Removes all covers of the @p track from the cache using settings for grouped thumbnails. */
    void removeFromCache(const Track& track, const SettingsManager& settings);

Q_SIGNALS:
    /** Emitted after cached artwork for @p track has changed or been invalidated. */
    void coverAdded(const Fooyin::Track& track);
    /** Emitted when the configured placeholder artwork changes. */
    void placeholderChanged();

private:
    std::unique_ptr<CoverProviderPrivate> p;
};
} // namespace Fooyin
