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

#pragma once

#include "fygui_export.h"

#include <core/track.h>

#include <QFuture>
#include <QObject>

#include <memory>
#include <set>

class QPixmap;
class QString;
class QSize;

namespace Fooyin {
class AudioLoader;
class SettingsManager;

/*!
 * Provides access to track album artwork.
 */
class FYGUI_EXPORT CoverProvider : public QObject
{
    Q_OBJECT

public:
    enum ThumbnailSize : uint16_t
    {
        None        = 0,
        Tiny        = 32,
        Small       = 64,
        MediumSmall = 96,
        Medium      = 128,
        Large       = 192,
        VeryLarge   = 256,
        ExtraLarge  = 512,
        Huge        = 768,
        Full        = 1024
    };

    explicit CoverProvider(std::shared_ptr<AudioLoader> audioLoader, SettingsManager* settings,
                           QObject* parent = nullptr);
    ~CoverProvider() override;

    /*!
     * If @c true, @fn trackCover methods will return a placeholder if tracks don't have covers or
     * are still being loaded.
     * @note this is enabled by default.
     */
    void setUsePlaceholder(bool enabled);

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
    [[nodiscard]] QPixmap placeholderCover() const;

    /** Returns an equivalent thumbnail size for the given @p size */
    static ThumbnailSize findThumbnailSize(const QSize& size);
    /** Clears the QPixmapCache as well as the on-disk cache. */
    static void clearCache();
    /** Removes all covers of the @p track from the cache. */
    static void removeFromCache(const Track& track);

signals:
    /** Emitted after a @fn trackCover or @fn trackCoverThumbnail call if and when the cover is added to the cache. */
    void coverAdded(const Fooyin::Track& track);

private:
    class CoverProviderPrivate;
    std::unique_ptr<CoverProviderPrivate> p;
    static std::set<QString> m_noCoverKeys;
};
} // namespace Fooyin
