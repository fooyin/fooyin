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

#include <gui/guiutils.h>

#include <core/coresettings.h>
#include <core/library/musiclibrary.h>
#include <core/library/tracksort.h>
#include <gui/guisettings.h>
#include <utils/datastream.h>
#include <utils/settings/settingsmanager.h>

#include <gui/widgets/expandedtreeview.h>

#include <QAbstractItemView>
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QIODevice>
#include <QLabel>
#include <QMimeData>
#include <QSet>
#include <QStyle>
#include <QTreeView>
#include <QUrl>

using namespace Qt::StringLiterals;

namespace Fooyin::Gui {
namespace {
QHeaderView* itemViewHeader(QAbstractItemView* view)
{
    if(auto* expandedView = qobject_cast<ExpandedTreeView*>(view)) {
        return expandedView->header();
    }
    if(auto* treeView = qobject_cast<QTreeView*>(view)) {
        return treeView->header();
    }
    return nullptr;
}
} // namespace

bool styleSupportsCustomPalette(const QString& styleName)
{
#ifdef Q_OS_WIN
    return styleName.compare("windows11"_L1, Qt::CaseInsensitive) != 0
        && styleName.compare("windowsvista"_L1, Qt::CaseInsensitive) != 0
        && styleName.compare("windows"_L1, Qt::CaseInsensitive) != 0;
#else
    Q_UNUSED(styleName)
    return true;
#endif
}

bool styleSupportsDarkMode(const QString& styleName)
{
#ifdef Q_OS_WIN
    return styleName.compare("windows11"_L1, Qt::CaseInsensitive) == 0
        || styleName.compare("windows"_L1, Qt::CaseInsensitive) == 0;
#else
    Q_UNUSED(styleName)
    return false;
#endif
}

bool styleUsesNormalItemViewSelectionText(const QString& styleName, bool alternatingRows)
{
    if(styleName.compare("windows11"_L1, Qt::CaseInsensitive) == 0) {
        // Windows 11 uses an accent selection with contrasting text for alternating rows
        return !alternatingRows;
    }
    return styleName.compare("windowsvista"_L1, Qt::CaseInsensitive) == 0;
}

QPalette::ColorRole itemViewSelectionTextRole(const QStyleOptionViewItem& option)
{
    const QStyle* style = option.widget ? option.widget->style() : QApplication::style();
    const auto* view    = qobject_cast<const QAbstractItemView*>(option.widget);
    const bool normalSelectionText
        = style && styleUsesNormalItemViewSelectionText(style->name(), view && view->alternatingRowColors());

    return normalSelectionText ? QPalette::Text : QPalette::HighlightedText;
}

QRect itemViewTextRect(const QStyleOptionViewItem& option)
{
    const QStyle* style     = option.widget ? option.widget->style() : QApplication::style();
    QRect textRect          = style->subElementRect(QStyle::SE_ItemViewItemText, &option, option.widget);
    const int textMargin    = style->pixelMetric(QStyle::PM_FocusFrameHMargin, &option, option.widget) + 1;
    const int frameWidth    = style->pixelMetric(QStyle::PM_DefaultFrameWidth, &option, option.widget);
    const int leadingInset  = textMargin + frameWidth + 1;
    const bool leadingCell  = option.viewItemPosition == QStyleOptionViewItem::Beginning
                           || option.viewItemPosition == QStyleOptionViewItem::OnlyOne
                           || option.viewItemPosition == QStyleOptionViewItem::Invalid;
    const int leadingMargin = leadingCell ? leadingInset : textMargin;

    if(option.direction == Qt::RightToLeft) {
        textRect.adjust(textMargin, 0, -leadingMargin, 0);
    }
    else {
        textRect.adjust(leadingMargin, 0, -textMargin, 0);
    }

    return textRect;
}

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
        stream << track.entryId;
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
        stream >> track.entryId;
        stream >> track.indexInPlaylist;

        track.track = library->trackForId(id);
        tracks.push_back(track);
    }

    return tracks;
}

RatingStarSymbols ratingStarSymbols(const SettingsManager& settings)
{
    return {
        .fullStarSymbol  = settings.value<Settings::Gui::RatingFullStarSymbol>(),
        .halfStarSymbol  = settings.value<Settings::Gui::RatingHalfStarSymbol>(),
        .emptyStarSymbol = settings.value<Settings::Gui::RatingEmptyStarSymbol>(),
    };
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

void refreshItemViewPalette(QAbstractItemView* view)
{
    refreshItemViewPalette(view, QApplication::palette());
}

void refreshItemViewPalette(QAbstractItemView* view, const QPalette& palette)
{
    if(!view) {
        return;
    }

    QPalette itemViewPalette{palette};
    if(const auto* style = view->style();
       style && styleUsesNormalItemViewSelectionText(style->name(), view->alternatingRowColors())) {
        for(const auto group : {QPalette::Active, QPalette::Disabled, QPalette::Inactive}) {
            itemViewPalette.setBrush(group, QPalette::HighlightedText, itemViewPalette.brush(group, QPalette::Text));
        }
    }

    view->setPalette(itemViewPalette);
    if(view->viewport()) {
        view->viewport()->setPalette(itemViewPalette);
    }
    if(auto* header = itemViewHeader(view)) {
        header->setPalette(itemViewPalette);
    }
}

void updateItemViewStyle(QAbstractItemView* view)
{
    updateItemViewStyle(view, QApplication::palette());
}

void updateItemViewStyle(QAbstractItemView* view, const QPalette& palette)
{
    refreshItemViewPalette(view, palette);

    if(!view) {
        return;
    }

    view->doItemsLayout();
    if(view->viewport()) {
        view->viewport()->update();
    }
    if(auto* header = itemViewHeader(view); header && header->viewport()) {
        header->viewport()->update();
    }
}

QLabel* createSectionHeader(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);

    QFont font{label->font()};
    font.setBold(true);
    label->setFont(font);

    return label;
}

} // namespace Fooyin::Gui
