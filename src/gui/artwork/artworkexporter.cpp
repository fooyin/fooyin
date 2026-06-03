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

#include "artworkexporter.h"

#include "sources/artworksource.h"

#include <core/engine/audioloader.h>
#include <utils/utils.h>

#include <QFileDialog>
#include <QFileInfo>
#include <QMainWindow>
#include <QMimeDatabase>
#include <QObject>
#include <ranges>

using namespace Qt::StringLiterals;

namespace {
QString exportDirectory(const Fooyin::Track& track)
{
    if(!track.isValid()) {
        return {};
    }

    if(track.isInArchive()) {
        return QFileInfo{track.archivePath()}.absolutePath();
    }

    return track.path();
}

QString exportKey(const Fooyin::Track& track, Fooyin::Track::Cover type)
{
    return QDir::cleanPath(exportDirectory(track)) + u'|' + QString::number(static_cast<int>(type));
}

QString exportBaseName(Fooyin::Track::Cover type)
{
    if(type == Fooyin::Track::Cover::Front) {
        return u"cover"_s;
    }
    if(type == Fooyin::Track::Cover::Back) {
        return u"back"_s;
    }
    if(type == Fooyin::Track::Cover::Artist) {
        return u"artist"_s;
    }
    return u"artwork"_s;
}

QString exportSuffix(const Fooyin::ArtworkResult& artwork)
{
    const QMimeDatabase mimeDb;

    if(!artwork.mimeType.isEmpty()) {
        QString suffix = mimeDb.mimeTypeForName(artwork.mimeType).preferredSuffix().toLower();
        if(!suffix.isEmpty()) {
            return suffix;
        }
    }

    QString suffix = mimeDb.mimeTypeForData(artwork.image).preferredSuffix().toLower();
    if(!suffix.isEmpty()) {
        return suffix;
    }

    return u"img"_s;
}

QString exportPath(const Fooyin::Track& track, Fooyin::Track::Cover type, const Fooyin::ArtworkResult& artwork,
                   QWidget* parent)
{
    const QString suffix        = exportSuffix(artwork);
    const QString exportDir     = exportDirectory(track);
    const QString suggestedPath = QDir::cleanPath(u"%1/%2.%3"_s.arg(exportDir, exportBaseName(type), suffix));
    const QString filter = QObject::tr("Image Files") + u" (*.%1);;"_s.arg(suffix) + QObject::tr("All Files (*)");

    auto* dialogParent = parent ? parent : Fooyin::Utils::getMainWindow();

    QFileDialog saveDialog(dialogParent, QObject::tr("Extract Artwork"), QFileInfo{suggestedPath}.absolutePath(),
                           filter);
    saveDialog.setAcceptMode(QFileDialog::AcceptSave);
    saveDialog.setFileMode(QFileDialog::AnyFile);
    saveDialog.setOption(QFileDialog::DontResolveSymlinks);
    saveDialog.setDefaultSuffix(suffix);
    saveDialog.selectFile(QFileInfo{suggestedPath}.fileName());

    if(saveDialog.exec() == 0) {
        return {};
    }

    const QStringList selectedFiles = saveDialog.selectedFiles();
    if(selectedFiles.isEmpty()) {
        return {};
    }

    return QDir::cleanPath(selectedFiles.constFirst());
}

Fooyin::ArtworkResult extractEmbeddedArtwork(Fooyin::AudioLoader* loader, const Fooyin::Track& track,
                                             Fooyin::Track::Cover type)
{
    const QByteArray image = loader->readTrackCover(track, type);
    if(image.isEmpty()) {
        return {};
    }

    const QMimeDatabase mimeDb;
    return {.mimeType = mimeDb.mimeTypeForData(image).name(), .image = image};
}

bool writeArtwork(const Fooyin::Track& track, Fooyin::Track::Cover type, const Fooyin::ArtworkResult& artwork)
{
    if(!track.isValid() || artwork.image.isEmpty()) {
        return false;
    }

    const QString dir = exportDirectory(track);
    if(dir.isEmpty()) {
        return false;
    }

    const QString path{u"%1/%2.%3"_s.arg(dir, exportBaseName(type), exportSuffix(artwork))};
    QFile file{QDir::cleanPath(path)};
    return file.open(QIODevice::WriteOnly) && file.write(artwork.image) == artwork.image.size();
}

bool writeArtwork(const QString& path, const Fooyin::ArtworkResult& artwork)
{
    if(path.isEmpty() || artwork.image.isEmpty()) {
        return false;
    }

    QFile file{QDir::cleanPath(path)};
    return file.open(QIODevice::WriteOnly) && file.write(artwork.image) == artwork.image.size();
}
} // namespace

namespace Fooyin {
ArtworkExportSummary ArtworkExporter::extractTracks(AudioLoader* loader, const TrackList& tracks,
                                                    const std::set<Track::Cover>& types)
{
    ArtworkExportSummary summary;

    if(!loader || tracks.empty() || types.empty()) {
        return summary;
    }

    std::set<QString> exported;

    for(const auto& track : std::ranges::reverse_view(tracks)) {
        if(!track.isValid()) {
            continue;
        }

        for(const auto type : types) {
            const QString key = exportKey(track, type);
            if(exported.contains(key)) {
                continue;
            }

            const ArtworkResult artwork = extractEmbeddedArtwork(loader, track, type);
            if(artwork.image.isEmpty()) {
                continue;
            }

            if(writeArtwork(track, type, artwork)) {
                ++summary.written;
            }
            else {
                ++summary.failed;
            }
            exported.emplace(key);
        }
    }

    return summary;
}

ArtworkExportSummary ArtworkExporter::extractTracks(const TrackList& tracks, Track::Cover type,
                                                    const ArtworkResult& artwork)
{
    ArtworkExportSummary summary;

    if(tracks.empty() || artwork.image.isEmpty()) {
        return summary;
    }

    std::set<QString> exported;

    for(const auto& track : std::ranges::reverse_view(tracks)) {
        if(!track.isValid()) {
            continue;
        }

        const QString key = exportKey(track, type);
        if(exported.contains(key)) {
            continue;
        }

        if(writeArtwork(track, type, artwork)) {
            ++summary.written;
        }
        else {
            ++summary.failed;
        }
        exported.emplace(key);
    }

    return summary;
}

QString ArtworkExporter::extractTrackAs(AudioLoader* loader, const Track& track, Track::Cover type, QWidget* parent)
{
    if(!loader || !track.isValid()) {
        return {};
    }

    return extractArtworkAs(track, type, extractEmbeddedArtwork(loader, track, type), parent);
}

QString ArtworkExporter::extractArtworkAs(const Track& track, Track::Cover type, const ArtworkResult& artwork,
                                          QWidget* parent)
{
    if(!track.isValid()) {
        return {};
    }

    if(artwork.image.isEmpty()) {
        return {};
    }

    const QString path = exportPath(track, type, artwork, parent);
    if(path.isEmpty() || !writeArtwork(path, artwork)) {
        return {};
    }

    return path;
}

QString ArtworkExporter::statusMessage(const ArtworkExportSummary& summary)
{
    if(summary.written == 0 && summary.failed == 0) {
        return QObject::tr("No embedded artwork found to extract");
    }
    if(summary.failed == 0) {
        return QObject::tr("Extracted artwork to %Ln file(s)", nullptr, summary.written);
    }
    if(summary.written == 0) {
        return QObject::tr("Failed to extract artwork");
    }
    return QObject::tr("Extracted artwork to %Ln file(s); some extractions failed", nullptr, summary.written);
}
} // namespace Fooyin
