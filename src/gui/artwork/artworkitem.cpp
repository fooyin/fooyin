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

#include "artworkitem.h"

#include <gui/guiconstants.h>
#include <utils/utils.h>

#include <QBuffer>
#include <QImageReader>
#include <QLoggingCategory>
#include <QMimeDatabase>

Q_LOGGING_CATEGORY(ARTWORK_ITEM, "fy.artworkitem")

namespace {
QSize calculateScaledSize(const QSize& originalSize, int maxSize)
{
    int newWidth{0};
    int newHeight{0};

    if(originalSize.width() > originalSize.height()) {
        newWidth  = maxSize;
        newHeight = (maxSize * originalSize.height()) / originalSize.width();
    }
    else {
        newHeight = maxSize;
        newWidth  = (maxSize * originalSize.width()) / originalSize.height();
    }

    return {newWidth, newHeight};
}
} // namespace

namespace Fooyin {
ArtworkItem::ArtworkItem(const SearchResult& result, int maxSize)
    : m_url{result.imageUrl}
    , m_title{result.artist}
    , m_isLoaded{false}
    , m_progress{0}
    , m_maxSize{maxSize}
{
    if(!result.album.isEmpty()) {
        if(!m_title.isEmpty()) {
            m_title += u"\n";
        }
        m_title += result.album;
    }
}

void ArtworkItem::load(const ArtworkResult& result)
{
    m_result   = result;
    m_image    = readImage(m_result.image);
    m_isLoaded = true;
}

QUrl ArtworkItem::url() const
{
    return m_url;
}

QString ArtworkItem::title() const
{
    return m_title;
}

bool ArtworkItem::isLoaded() const
{
    return m_isLoaded;
}

QSize ArtworkItem::size() const
{
    return m_image.size;
}

ArtworkResult ArtworkItem::result() const
{
    return m_result;
}

QPixmap ArtworkItem::thumbnail() const
{
    if(!m_isLoaded) {
        return Utils::pixmapFromTheme(Constants::Icons::NoCover, {m_maxSize, m_maxSize});
    }
    return m_image.thumbnail;
}

int ArtworkItem::progress() const
{
    return m_progress;
}

void ArtworkItem::setProgress(int progress)
{
    m_progress = progress;
}

ArtworkItem::ImageData ArtworkItem::readImage(QByteArray data)
{
    QBuffer buffer{&data};
    const QMimeDatabase mimeDb;
    const auto mimeType   = mimeDb.mimeTypeForData(&buffer);
    const auto formatHint = mimeType.preferredSuffix().toLocal8Bit().toLower();

    QImageReader reader{&buffer, formatHint};

    if(!reader.canRead()) {
        qCDebug(ARTWORK_ITEM) << "Failed to use format hint" << formatHint << "when trying to load cover data";

        reader.setFormat({});
        reader.setDevice(&buffer);
        if(!reader.canRead()) {
            qCDebug(ARTWORK_ITEM) << "Failed to load cover";
            return {};
        }
    }

    const auto size = reader.size();
    if(size.width() > m_maxSize || size.height() > m_maxSize) {
        const auto scaledSize = calculateScaledSize(size, m_maxSize);
        reader.setScaledSize(scaledSize);
    }

    return {.thumbnail = QPixmap::fromImage(reader.read()), .size = size};
}
} // namespace Fooyin
