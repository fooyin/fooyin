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

#include <QObject>

#include <memory>

class QPixmap;
class QString;
class QSize;

namespace Fooyin {
/*!
 * Provides access to track album artwork.
 */
class FYGUI_EXPORT CoverProvider : public QObject
{
    Q_OBJECT

public:
    explicit CoverProvider(QObject* parent = nullptr);
    ~CoverProvider() override;

    /*!
     * If @c true, @fn trackCover methods will return a placeholder if tracks don't have covers or
     * are still being loaded.
     * @note this is enabled by default.
     */
    void setUsePlaceholder(bool enabled);
    /*!
     * Sets the key to be used when saving to cache/disk.
     * Useful for keeping a temporary buffer of a single cover.
     * @note the album hash of a track is used by default.
     * @note use @fn resetCoverKey to restore.
     */
    void setCoverKey(const QString& name);
    /** Restores default behaviour of using a track's album hash as the key. */
    void resetCoverKey();

    /*!
     * This will return either the front cover picture of the @p track if it's either:
     * - Non-embedded.
     * - Exists in QPixmapCache.
     * Or it will return a QPixmap representing 'no cover' and the cover will be read asynchronously from
     * the @p track metadata.
     * Once loaded, the coverAdded signal will be emitted with the same track. It is then up to the caller
     * to query the cover using this method again.
     *
     * @param track the track for which the cover will be found.
     * @param saveToDisk whether the cover will be saved to the on-disk cache.
     * @returns the cover if already in the cache or on disk, the 'no cover' cover if not.
     */
    [[nodiscard]] QPixmap trackCover(const Track& track, const QSize& size, bool saveToDisk = false) const;

    /*!
     * This is an overloaded function.
     * Returns the front cover picture of the @p track up to a maximum size (800px).
     * @see trackCover(const Track&, const QSize&, bool).
     */
    [[nodiscard]] QPixmap trackCover(const Track& track, bool saveToDisk = false) const;

    /** Clears the QPixmapCache as well as the on-disk cache. */
    void clearCache();
    /** Removes the cover with the @p key from the cache. */
    void removeFromCache(const QString& key);

signals:
    /** Emitted after a @fn trackCover call if the cover needs to be read from the track metadata. */
    void coverAdded(const Fooyin::Track& track);

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin
