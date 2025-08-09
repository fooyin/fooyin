/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "sources/artworksource.h"

#include <QPixmap>

namespace Fooyin {
struct SearchResult;

class ArtworkItem
{
public:
    enum Role : uint16_t
    {
        Result = Qt::UserRole,
        Caption,
        IsLoaded
    };

    explicit ArtworkItem(const SearchResult& result, int maxSize);

    void load(const ArtworkResult& result);

    [[nodiscard]] QUrl url() const;
    [[nodiscard]] QString title() const;

    [[nodiscard]] bool isLoaded() const;
    [[nodiscard]] QSize size() const;
    [[nodiscard]] ArtworkResult result() const;
    [[nodiscard]] QPixmap thumbnail() const;
    [[nodiscard]] int progress() const;

    void setProgress(int progress);

private:
    struct ImageData
    {
        QPixmap thumbnail;
        QSize size;
    };

    ImageData readImage(QByteArray data);

    QUrl m_url;
    QString m_title;
    bool m_isLoaded;
    ArtworkResult m_result;

    ImageData m_image;
    int m_progress;
    int m_maxSize;
};
} // namespace Fooyin
