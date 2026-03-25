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

#include <gui/guiutils.h>

#include <core/coresettings.h>
#include <core/library/musiclibrary.h>
#include <core/library/tracksort.h>
#include <utils/datastream.h>
#include <utils/settings/settingsmanager.h>

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QIODevice>
#include <QMimeData>
#include <QSet>
#include <QStyle>
#include <QUrl>

namespace Fooyin::Gui {
TrackList tracksFromMimeData(MusicLibrary* library, QByteArray data)
{
    QDataStream stream(&data, QIODevice::ReadOnly);

    TrackIds ids;
    stream >> ids;
    TrackList tracks = library->tracksForIds(ids);

    return tracks;
}

void populateExternalTrackMimeData(const TrackList& tracks, QMimeData* mimeData)
{
    if(!mimeData) {
        return;
    }

    QList<QUrl> urls;
    QStringList paths;
    QSet<QString> seenPaths;

    for(const Track& track : tracks) {
        if(!track.isValid() || track.isInArchive()) {
            continue;
        }

        const QFileInfo fileInfo{track.filepath()};
        if(!fileInfo.exists() || !fileInfo.isFile()) {
            continue;
        }

        const QString path = QDir::cleanPath(fileInfo.absoluteFilePath());
        if(path.isEmpty() || seenPaths.contains(path)) {
            continue;
        }

        seenPaths.insert(path);
        urls.push_back(QUrl::fromLocalFile(path));
        paths.push_back(path);
    }

    if(urls.empty()) {
        return;
    }

    mimeData->setUrls(urls);
    mimeData->setText(paths.join(QStringLiteral("\n")));
}

TrackList sortTracksForLibraryViewerPlaylist(SettingsManager* settings, const TrackList& tracks)
{
    if(!settings || tracks.size() < 2) {
        return tracks;
    }

    const QString sortScript = settings->value<Settings::Core::LibraryViewPlaylistSortScript>();
    if(sortScript.isEmpty()) {
        return tracks;
    }

    TrackSorter sorter;
    return sorter.calcSortTracks(sortScript, tracks);
}

TrackIds sortTrackIdsForLibraryViewerPlaylist(MusicLibrary* library, SettingsManager* settings, const TrackIds& ids)
{
    if(!library || ids.size() < 2) {
        return ids;
    }

    const TrackList sortedTracks = sortTracksForLibraryViewerPlaylist(settings, library->tracksForIds(ids));
    if(sortedTracks.size() != ids.size()) {
        return ids;
    }

    TrackIds sortedIds;
    sortedIds.reserve(sortedTracks.size());

    for(const Track& track : sortedTracks) {
        sortedIds.push_back(track.id());
    }

    return sortedIds;
}

QByteArray queueTracksToMimeData(const QueueTracks& tracks)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);

    for(const auto& track : tracks) {
        stream << track.track.id();
        stream << track.playlistId;
        stream << track.indexInPlaylist;
    }

    return data;
}

QueueTracks queueTracksFromMimeData(MusicLibrary* library, QByteArray data)
{
    QDataStream stream(&data, QIODevice::ReadOnly);

    QueueTracks tracks;

    while(!stream.atEnd()) {
        PlaylistTrack track;

        int id{-1};
        stream >> id;
        stream >> track.playlistId;
        stream >> track.indexInPlaylist;

        track.track = library->trackForId(id);
        tracks.push_back(track);
    }

    return tracks;
}

QMap<PaletteKey, QColor> coloursFromPalette()
{
    return coloursFromPalette(QApplication::palette());
}

QMap<PaletteKey, QColor> coloursFromStylePalette()
{
    return coloursFromPalette(QApplication::style()->standardPalette());
}

QMap<PaletteKey, QColor> coloursFromPalette(const QPalette& palette)
{
    QMap<PaletteKey, QColor> colours;

    using P = QPalette;

    colours[PaletteKey{P::WindowText}]                   = palette.color(P::Active, P::WindowText);
    colours[PaletteKey{P::WindowText, P::Disabled}]      = palette.color(P::Disabled, P::WindowText);
    colours[PaletteKey{P::Button}]                       = palette.color(P::Active, P::Button);
    colours[PaletteKey{P::Light}]                        = palette.color(P::Active, P::Light);
    colours[PaletteKey{P::Midlight}]                     = palette.color(P::Active, P::Midlight);
    colours[PaletteKey{P::Dark}]                         = palette.color(P::Active, P::Dark);
    colours[PaletteKey{P::Mid}]                          = palette.color(P::Active, P::Mid);
    colours[PaletteKey{P::Text}]                         = palette.color(P::Active, P::Text);
    colours[PaletteKey{P::Text, P::Disabled}]            = palette.color(P::Disabled, P::Text);
    colours[PaletteKey{P::BrightText}]                   = palette.color(P::Active, P::BrightText);
    colours[PaletteKey{P::ButtonText}]                   = palette.color(P::Active, P::ButtonText);
    colours[PaletteKey{P::ButtonText, P::Disabled}]      = palette.color(P::Disabled, P::ButtonText);
    colours[PaletteKey{P::Base}]                         = palette.color(P::Active, P::Base);
    colours[PaletteKey{P::Window}]                       = palette.color(P::Active, P::Window);
    colours[PaletteKey{P::Shadow}]                       = palette.color(P::Active, P::Shadow);
    colours[PaletteKey{P::Highlight}]                    = palette.color(P::Active, P::Highlight);
    colours[PaletteKey{P::Highlight, P::Disabled}]       = palette.color(P::Disabled, P::Highlight);
    colours[PaletteKey{P::HighlightedText}]              = palette.color(P::Active, P::HighlightedText);
    colours[PaletteKey{P::HighlightedText, P::Disabled}] = palette.color(P::Disabled, P::HighlightedText);
    colours[PaletteKey{P::Link}]                         = palette.color(P::Active, P::Link);
    colours[PaletteKey{P::LinkVisited}]                  = palette.color(P::Active, P::LinkVisited);
    colours[PaletteKey{P::AlternateBase}]                = palette.color(P::Active, P::AlternateBase);
    colours[PaletteKey{P::ToolTipBase}]                  = palette.color(P::Active, P::ToolTipBase);
    colours[PaletteKey{P::ToolTipText}]                  = palette.color(P::Active, P::ToolTipText);
    colours[PaletteKey{P::PlaceholderText}]              = palette.color(P::Active, P::PlaceholderText);

    return colours;
}
} // namespace Fooyin::Gui
