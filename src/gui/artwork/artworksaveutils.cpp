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

#include "artworksaveutils.h"

#include "sources/artworksource.h"

#include <QBuffer>
#include <QImageWriter>
#include <QPainter>
#include <algorithm>

using namespace Qt::StringLiterals;

namespace {
QString normaliseArtworkFormat(const QString& format)
{
    return format.trimmed().toLower();
}

QString saveFormatFromMimeType(const QString& mimeType)
{
    if(mimeType == "image/jpeg"_L1 || mimeType == "image/jpg"_L1) {
        return u"jpeg"_s;
    }
    if(mimeType == "image/png"_L1) {
        return u"png"_s;
    }
    if(mimeType == "image/webp"_L1) {
        return u"webp"_s;
    }

    return {};
}

QByteArray writerFormat(const QString& format)
{
    const QString normalised = normaliseArtworkFormat(format);

    if(normalised == "jpeg"_L1) {
        return "jpeg";
    }
    if(normalised == "png"_L1) {
        return "png";
    }
    if(normalised == "webp"_L1) {
        return "webp";
    }

    return {};
}

QString mimeTypeForFormat(const QString& format)
{
    const QString normalised = normaliseArtworkFormat(format);

    if(normalised == "jpeg"_L1) {
        return u"image/jpeg"_s;
    }
    if(normalised == "png"_L1) {
        return u"image/png"_s;
    }
    if(normalised == "webp"_L1) {
        return u"image/webp"_s;
    }

    return {};
}

QString suffixForFormat(const QString& format)
{
    const QString normalised = normaliseArtworkFormat(format);

    if(normalised == "jpeg"_L1) {
        return u"jpg"_s;
    }
    if(normalised == "png"_L1) {
        return u"png"_s;
    }
    if(normalised == "webp"_L1) {
        return u"webp"_s;
    }

    return {};
}

bool isWriterFormatSupported(const QString& format)
{
    const QByteArray desiredFormat = writerFormat(format);
    if(desiredFormat.isEmpty()) {
        return false;
    }

    const auto supportedFormats = QImageWriter::supportedImageFormats();
    return std::ranges::any_of(supportedFormats, [&desiredFormat](const auto& supportedFormat) {
        return supportedFormat.toLower() == desiredFormat;
    });
}

QImage flattenImageIfNeeded(const QImage& image, const QString& format)
{
    if(normaliseArtworkFormat(format) != "jpeg"_L1 || !image.hasAlphaChannel()) {
        return image;
    }

    QImage flattened{image.size(), QImage::Format_RGB32};
    flattened.fill(Qt::white);

    QPainter painter{&flattened};
    painter.drawImage(QPoint{}, image);

    return flattened;
}
} // namespace

namespace Fooyin {
QStringList supportedArtworkSaveFormats()
{
    QStringList formats;

    for(const QString& format : {u"jpeg"_s, u"png"_s, u"webp"_s}) {
        if(isWriterFormatSupported(format)) {
            formats.append(format);
        }
    }

    return formats;
}

QString artworkSaveFormatLabel(const QString& format)
{
    const QString normalised = normaliseArtworkFormat(format);
    if(normalised == "jpeg"_L1) {
        return u"JPEG"_s;
    }
    if(normalised == "png"_L1) {
        return u"PNG"_s;
    }
    if(normalised == "webp"_L1) {
        return u"WebP"_s;
    }
    return format.toUpper();
}

ArtworkSaveResult prepareArtworkForSave(const ArtworkResult& result, const QString& preferredFormat, int quality)
{
    const QString currentFormat = saveFormatFromMimeType(result.mimeType);
    const QString targetFormat  = normaliseArtworkFormat(preferredFormat);

    ArtworkSaveResult saveResult{
        .image    = result.image,
        .mimeType = result.mimeType,
        .suffix   = suffixForFormat(currentFormat),
    };

    if(saveResult.suffix.isEmpty()) {
        saveResult.suffix = u"img"_s;
    }

    if(targetFormat.isEmpty() || targetFormat == currentFormat || !isWriterFormatSupported(targetFormat)) {
        return saveResult;
    }

    QImage image;
    if(!image.loadFromData(result.image)) {
        return saveResult;
    }

    QByteArray encodedImage;
    QBuffer buffer{&encodedImage};
    if(!buffer.open(QIODevice::WriteOnly)) {
        return saveResult;
    }

    QImageWriter writer{&buffer, writerFormat(targetFormat)};
    if(targetFormat == "jpeg"_L1 || targetFormat == "webp"_L1) {
        writer.setQuality(std::clamp(quality, 0, 100));
    }

    if(!writer.write(flattenImageIfNeeded(image, targetFormat))) {
        return saveResult;
    }

    saveResult.image    = std::move(encodedImage);
    saveResult.mimeType = mimeTypeForFormat(targetFormat);
    saveResult.suffix   = suffixForFormat(targetFormat);

    return saveResult;
}
} // namespace Fooyin
