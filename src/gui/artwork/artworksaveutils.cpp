/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include <array>

using namespace Qt::StringLiterals;

namespace {
struct ArtworkSaveFormat
{
    QLatin1StringView name;
    QLatin1StringView mimeType;
    QLatin1StringView suffix;
    QLatin1StringView label;
};

constexpr std::array ArtworkSaveFormats{
    ArtworkSaveFormat{.name = "jpeg"_L1, .mimeType = "image/jpeg"_L1, .suffix = "jpg"_L1, .label = "JPEG"_L1},
    ArtworkSaveFormat{.name = "png"_L1, .mimeType = "image/png"_L1, .suffix = "png"_L1, .label = "PNG"_L1},
    ArtworkSaveFormat{.name = "webp"_L1, .mimeType = "image/webp"_L1, .suffix = "webp"_L1, .label = "WebP"_L1},
};

const ArtworkSaveFormat* findArtworkSaveFormat(const QString& format)
{
    const auto it = std::ranges::find(ArtworkSaveFormats, format.trimmed().toLower(), &ArtworkSaveFormat::name);
    return it != ArtworkSaveFormats.cend() ? &*it : nullptr;
}

const ArtworkSaveFormat* findArtworkSaveFormatForMimeType(const QString& mimeType)
{
    const auto canonicalMimeType = mimeType == "image/jpg"_L1 ? u"image/jpeg"_s : mimeType;
    const auto it = std::ranges::find(ArtworkSaveFormats, canonicalMimeType, &ArtworkSaveFormat::mimeType);
    return it != ArtworkSaveFormats.cend() ? &*it : nullptr;
}

QByteArray writerFormat(const ArtworkSaveFormat& format)
{
    return {format.name.data(), format.name.size()};
}

bool isWriterFormatSupported(const ArtworkSaveFormat& format)
{
    const QByteArray desiredFormat = writerFormat(format);
    const auto supportedFormats    = QImageWriter::supportedImageFormats();
    return std::ranges::any_of(supportedFormats, [&desiredFormat](const auto& supportedFormat) {
        return supportedFormat.toLower() == desiredFormat;
    });
}

QImage flattenImageIfNeeded(const QImage& image, QLatin1StringView format)
{
    if(format != "jpeg"_L1 || !image.hasAlphaChannel()) {
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

    for(const auto& format : ArtworkSaveFormats) {
        if(isWriterFormatSupported(format)) {
            formats.append(QString{format.name});
        }
    }

    return formats;
}

QString artworkSaveFormatLabel(const QString& format)
{
    if(const auto* saveFormat = findArtworkSaveFormat(format)) {
        return QString{saveFormat->label};
    }
    return format.toUpper();
}

ArtworkSaveResult prepareArtworkForSave(const ArtworkResult& result, const QString& preferredFormat, int quality)
{
    const auto* currentFormat = findArtworkSaveFormatForMimeType(result.mimeType);
    const auto* targetFormat  = findArtworkSaveFormat(preferredFormat);

    ArtworkSaveResult saveResult{
        .image    = result.image,
        .mimeType = result.mimeType,
        .suffix   = currentFormat ? QString{currentFormat->suffix} : u"img"_s,
    };

    if(!targetFormat || targetFormat == currentFormat || !isWriterFormatSupported(*targetFormat)) {
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

    QImageWriter writer{&buffer, writerFormat(*targetFormat)};
    if(targetFormat->name == "jpeg"_L1 || targetFormat->name == "webp"_L1) {
        writer.setQuality(std::clamp(quality, 0, 100));
    }

    if(!writer.write(flattenImageIfNeeded(image, targetFormat->name))) {
        return saveResult;
    }

    saveResult.image    = std::move(encodedImage);
    saveResult.mimeType = QString{targetFormat->mimeType};
    saveResult.suffix   = QString{targetFormat->suffix};

    return saveResult;
}
} // namespace Fooyin
