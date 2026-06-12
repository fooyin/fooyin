/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "radiobrowsermodel.h"

#include "radiobrowserutils.h"
#include "radiodiscovery.h"
#include "radioiconprovider.h"

#include <core/track.h>
#include <gui/guiconstants.h>
#include <gui/trackmimedata.h>
#include <utils/stringcollator.h>
#include <utils/utils.h>

#include <QApplication>
#include <QIcon>
#include <QMimeData>
#include <QPalette>

#include <set>
#include <utility>

using namespace Qt::StringLiterals;

constexpr auto StationRowsMimeType       = "application/x-fooyin-radio-browser-station-rows"_L1;
constexpr auto MaxCachedPlaceholderIcons = 512;
constexpr auto DefaultRadioIconSize      = 36;

namespace Fooyin::RadioBrowser {
namespace {
Track trackForStation(const RadioStation& station)
{
    Track track{station.effectiveStreamUrl()};
    track.setTitle(station.name);

    if(!station.tags.isEmpty()) {
        track.setGenres(splitStationTags(station.tags));
    }
    if(!station.codec.isEmpty()) {
        track.setCodec(station.codec);
    }
    if(station.bitrate > 0) {
        track.setBitrate(station.bitrate);
    }
    if(!station.favicon.isEmpty()) {
        track.replaceExtraTag(u"COVERART"_s, station.favicon);
    }

    track.setMetadataWasRead(true);
    track.setExtraProperty(u"_STREAM_PROVIDER"_s, station.local ? u"local"_s : u"radio-browser.info"_s);
    track.setExtraProperty(u"_RADIO_BROWSER_STATIONUUID"_s, station.uuid);
    track.setExtraProperty(u"_RADIO_BROWSER_STREAMURL"_s, station.streamUrl);
    track.setExtraProperty(u"_RADIO_BROWSER_RESOLVED_STREAMURL"_s, station.resolvedStreamUrl);
    track.setExtraProperty(u"_RADIO_BROWSER_HOMEPAGE"_s, station.homepage);
    track.setExtraProperty(u"_RADIO_BROWSER_FAVICON"_s, station.favicon);
    track.setExtraProperty(u"_RADIO_BROWSER_HLS"_s, station.hls ? u"true"_s : u"false"_s);

    return track;
}

QString stationColumnText(const RadioStation& station, int column)
{
    switch(column) {
        case Station:
            return station.name;
        case Country:
            return displayNameForCountry(station.country, station.countryCode);
        case Language:
            return station.language;
        case Codec:
            return station.codec;
        case Bitrate:
            return station.bitrate > 0 ? RadioBrowserModel::tr("%1 kbps").arg(station.bitrate) : QString{};
        case Votes:
            return station.voteCount > 0 ? QString::number(station.voteCount) : QString{};
        case Clicks:
            return station.clickCount > 0 ? QString::number(station.clickCount) : QString{};
        case Tags:
            return station.tags;
        default:
            break;
    }

    return {};
}

std::vector<int> decodeRows(const QMimeData* data)
{
    if(!data || !data->hasFormat(StationRowsMimeType)) {
        return {};
    }

    std::vector<int> rows;
    std::set<int> seenRows;

    const QStringList parts = QString::fromUtf8(data->data(StationRowsMimeType)).split(u',', Qt::SkipEmptyParts);
    for(const QString& part : parts) {
        bool ok{false};
        const int row = part.toInt(&ok);
        if(ok && !seenRows.contains(row)) {
            seenRows.emplace(row);
            rows.emplace_back(row);
        }
    }

    std::ranges::sort(rows);
    return rows;
}

QColor playingRowColor()
{
    QColor base = QApplication::palette().color(QPalette::Inactive, QPalette::Highlight);

    if(Fooyin::Utils::isDarkMode()) {
        base = base.lighter(150);
    }
    else {
        base = base.darker(150);
    }

    base.setAlpha(110);
    return base;
}

QColor columnPlayingRowColor()
{
    QColor color = playingRowColor();
    color.setAlpha(55);
    return color;
}

int radioIconBucketSize(const QSize& size)
{
    const int logicalSize = std::max({size.width(), size.height(), DefaultRadioIconSize});
    const int targetSize  = logicalSize * 2;

    if(targetSize <= 64) {
        return 64;
    }
    if(targetSize <= 96) {
        return 96;
    }
    if(targetSize <= 128) {
        return 128;
    }
    if(targetSize <= 192) {
        return 192;
    }
    if(targetSize <= 256) {
        return 256;
    }
    return 384;
}
} // namespace

RadioBrowserModel::RadioBrowserModel(QObject* parent)
    : QAbstractTableModel{parent}
    , m_playingColour{columnPlayingRowColor()}
    , m_iconPlayingColour{playingRowColor()}
    , m_iconProvider{nullptr}
    , m_placeholderIcons{MaxCachedPlaceholderIcons}
    , m_iconSize{DefaultRadioIconSize, DefaultRadioIconSize}
    , m_iconCaptionLineCount{1}
    , m_rowHeight{0}
    , m_sortColumn{1}
    , m_sortOrder{Qt::AscendingOrder}
    , m_reorderEnabled{false}
    , m_apiSortingEnabled{false}
    , m_showIcons{true}
    , m_showToolTips{true}
    , m_showSavedIndicators{true}
{ }

int RadioBrowserModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_stations.size());
}

int RadioBrowserModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : Count;
}

QVariant RadioBrowserModel::data(const QModelIndex& index, const int role) const
{
    if(!index.isValid() || index.row() < 0 || std::cmp_greater_equal(index.row(), m_stations.size())) {
        return {};
    }

    const RadioStation& station = m_stations.at(index.row());

    if(role == Qt::SizeHintRole && m_rowHeight > 0) {
        return QSize{0, m_rowHeight};
    }
    if(isCurrentStation(station)) {
        if(role == Qt::BackgroundRole) {
            return m_playingColour;
        }
        if(role == IconBackgroundRole) {
            return m_iconPlayingColour;
        }
    }

    switch(role) {
        case NameRole:
            return station.name;
        case CountryRole:
            return displayNameForCountry(station.country, station.countryCode);
        case LanguageRole:
            return station.language;
        case CodecRole:
            return station.codec;
        case BitrateRole:
            return station.bitrate;
        case ClickCountRole:
            return station.clickCount;
        case VoteCountRole:
            return station.voteCount;
        case TagsRole:
            return station.tags;
        case FaviconRole:
            return station.favicon;
        case IconCaptionLineCountRole:
            return m_iconCaptionLineCount;
        case IconCaptionLinesRole: {
            QStringList lines;

            const auto appendColumn = [&lines, &station](int column) {
                const QString text = stationColumnText(station, column).trimmed();
                if(!text.isEmpty()) {
                    lines.emplace_back(text);
                }
            };

            if(m_iconColumnOrder.empty()) {
                appendColumn(Station);
            }
            else {
                for(const int column : m_iconColumnOrder) {
                    appendColumn(column);
                }
            }
            return lines;
        }
        case SavedStationRole:
            return m_showSavedIndicators && isSavedStation(station);
        default:
            break;
    }

    if(role == Qt::DisplayRole) {
        switch(index.column()) {
            case Station:
            case Country:
            case Language:
            case Codec:
            case Bitrate:
            case Votes:
            case Clicks:
            case Tags:
                return stationColumnText(station, index.column());
            default:
                break;
        }
    }

    if(role == Qt::ToolTipRole && m_showToolTips) {
        QStringList lines;
        lines.emplace_back(station.name);
        if(!station.country.isEmpty()) {
            lines.emplace_back(station.country);
        }
        if(!station.homepage.isEmpty()) {
            lines.emplace_back(station.homepage);
        }
        return lines.join(u'\n');
    }

    if(role == Qt::DecorationRole && m_showIcons && index.column() == Station) {
        return stationIcon(station);
    }

    return {};
}

Qt::ItemFlags RadioBrowserModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags flags = QAbstractTableModel::flags(index);

    if(index.isValid()) {
        flags |= Qt::ItemIsDragEnabled;
    }
    if(m_reorderEnabled) {
        flags |= Qt::ItemIsDropEnabled;
    }
    return flags;
}

QStringList RadioBrowserModel::mimeTypes() const
{
    return {StationRowsMimeType, QString::fromLatin1(Fooyin::Constants::Mime::Tracks)};
}

QMimeData* RadioBrowserModel::mimeData(const QModelIndexList& indexes) const
{
    std::vector<int> rows;
    std::set<int> seenRows;

    for(const QModelIndex& index : indexes) {
        const int row = index.row();
        if(index.isValid() && row >= 0 && std::cmp_less(row, m_stations.size()) && !seenRows.contains(row)) {
            rows.push_back(row);
            seenRows.insert(row);
        }
    }

    if(rows.empty()) {
        return nullptr;
    }

    std::ranges::sort(rows);

    TrackList tracks;
    tracks.reserve(rows.size());

    for(const int row : rows) {
        if(Track track = trackForStation(m_stations.at(row)); track.isValid()) {
            tracks.push_back(std::move(track));
        }
    }

    if(tracks.empty()) {
        return nullptr;
    }

    auto* data = new TrackMimeData{std::move(tracks)};

    if(m_reorderEnabled) {
        QStringList encodedRows;
        encodedRows.reserve(static_cast<qsizetype>(rows.size()));
        for(const int row : rows) {
            encodedRows.push_back(QString::number(row));
        }
        data->setData(StationRowsMimeType, encodedRows.join(u',').toUtf8());
    }

    return data;
}

bool RadioBrowserModel::dropMimeData(const QMimeData* data, const Qt::DropAction action, int row, const int column,
                                     const QModelIndex& parent)
{
    if(!m_reorderEnabled || action != Qt::MoveAction || column > 0) {
        return false;
    }

    std::vector<int> rows = decodeRows(data);
    if(rows.empty()) {
        return false;
    }

    rows.erase(std::ranges::remove_if(rows,
                                      [this](const int sourceRow) {
                                          return sourceRow < 0 || std::cmp_greater_equal(sourceRow, m_stations.size());
                                      })
                   .begin(),
               rows.end());
    if(rows.empty()) {
        return false;
    }

    if(row < 0) {
        row = parent.isValid() ? parent.row() : static_cast<int>(m_stations.size());
    }
    row = std::clamp(row, 0, static_cast<int>(m_stations.size()));

    int adjustedRow{row};
    for(const int sourceRow : rows) {
        if(sourceRow < row) {
            --adjustedRow;
        }
    }

    RadioStationList movedStations;
    movedStations.reserve(rows.size());
    for(const int sourceRow : rows) {
        movedStations.push_back(m_stations.at(sourceRow));
    }

    RadioStationList reorderedStations;
    reorderedStations.reserve(m_stations.size());

    int nextMovedIndex{0};
    for(int sourceRow{0}; std::cmp_less(sourceRow, m_stations.size()); ++sourceRow) {
        if(std::cmp_less(nextMovedIndex, rows.size()) && rows.at(nextMovedIndex) == sourceRow) {
            ++nextMovedIndex;
            continue;
        }
        reorderedStations.push_back(m_stations.at(sourceRow));
    }

    adjustedRow = std::clamp(adjustedRow, 0, static_cast<int>(reorderedStations.size()));
    reorderedStations.insert(reorderedStations.begin() + adjustedRow, movedStations.cbegin(), movedStations.cend());

    beginResetModel();
    m_stations = std::move(reorderedStations);
    endResetModel();

    Q_EMIT stationsReordered(m_stations);
    return true;
}

Qt::DropActions RadioBrowserModel::supportedDropActions() const
{
    return m_reorderEnabled ? Qt::MoveAction : Qt::IgnoreAction;
}

Qt::DropActions RadioBrowserModel::supportedDragActions() const
{
    return Qt::CopyAction | (m_reorderEnabled ? Qt::MoveAction : Qt::DropActions{});
}

QVariant RadioBrowserModel::headerData(const int section, const Qt::Orientation orientation, const int role) const
{
    if(orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QAbstractTableModel::headerData(section, orientation, role);
    }

    switch(section) {
        case Station:
            return tr("Station");
        case Country:
            return tr("Country");
        case Language:
            return tr("Language");
        case Codec:
            return tr("Codec");
        case Bitrate:
            return tr("Bitrate");
        case Votes:
            return tr("Votes");
        case Clicks:
            return tr("Clicks");
        case Tags:
            return tr("Tags");
        default:
            break;
    }

    return {};
}

void RadioBrowserModel::setIconProvider(RadioIconProvider* provider)
{
    if(m_iconProvider == provider) {
        return;
    }

    if(m_iconProvider) {
        QObject::disconnect(m_iconProvider, nullptr, this, nullptr);
    }

    m_iconProvider = provider;
    if(m_iconProvider) {
        QObject::connect(m_iconProvider, &RadioIconProvider::iconLoaded, this, &RadioBrowserModel::handleIconLoaded);
    }
}

void RadioBrowserModel::setStations(const RadioStationList& stations)
{
    beginResetModel();
    m_stations = stations;
    m_placeholderIcons.clear();
    if(!m_apiSortingEnabled) {
        sortStations();
    }
    updateIconCaptionLineCount();
    endResetModel();
}

void RadioBrowserModel::appendStations(const RadioStationList& stations)
{
    if(stations.empty()) {
        return;
    }

    if(!m_apiSortingEnabled && m_sortColumn >= 0) {
        beginResetModel();
        m_stations.insert(m_stations.end(), stations.cbegin(), stations.cend());
        sortStations();
        updateIconCaptionLineCount();
        endResetModel();
        return;
    }

    const int oldCaptionLineCount = m_iconCaptionLineCount;
    const int firstRow            = static_cast<int>(m_stations.size());
    const int lastRow             = firstRow + static_cast<int>(stations.size()) - 1;
    beginInsertRows({}, firstRow, lastRow);
    m_stations.insert(m_stations.end(), stations.cbegin(), stations.cend());
    updateIconCaptionLineCount();
    endInsertRows();

    if(oldCaptionLineCount != m_iconCaptionLineCount && firstRow > 0) {
        Q_EMIT dataChanged(index(0, 0), index(firstRow - 1, 0), {IconCaptionLineCountRole, Qt::SizeHintRole});
    }
}

void RadioBrowserModel::setIconColumnOrder(const std::vector<int>& order)
{
    if(m_iconColumnOrder == order) {
        return;
    }

    m_iconColumnOrder = order;
    updateIconCaptionLineCount();
    if(rowCount({}) > 0) {
        Q_EMIT dataChanged(index(0, 0), index(rowCount({}) - 1, 0),
                           {IconCaptionLinesRole, IconCaptionLineCountRole, Qt::SizeHintRole});
    }
}

void RadioBrowserModel::setIconSize(const QSize& size)
{
    const QSize iconSize = size.isValid() ? size : QSize{DefaultRadioIconSize, DefaultRadioIconSize};
    const int oldBucket  = iconBucketSize();
    m_iconSize           = iconSize;

    if(iconBucketSize() == oldBucket) {
        return;
    }

    m_placeholderIcons.clear();
    if(rowCount({}) > 0) {
        Q_EMIT dataChanged(index(0, Station), index(rowCount({}) - 1, Station), {Qt::DecorationRole});
    }
}

void RadioBrowserModel::setRowHeight(const int height)
{
    const int rowHeight = std::max(height, 0);
    if(std::exchange(m_rowHeight, rowHeight) == rowHeight) {
        return;
    }

    if(rowCount({}) > 0) {
        Q_EMIT dataChanged(index(0, 0), index(rowCount({}) - 1, columnCount({}) - 1), {Qt::SizeHintRole});
    }
}

void RadioBrowserModel::setShowIcons(const bool showIcons)
{
    if(std::exchange(m_showIcons, showIcons) == showIcons) {
        return;
    }

    if(rowCount({}) > 0) {
        Q_EMIT dataChanged(index(0, Station), index(rowCount({}) - 1, Station), {Qt::DecorationRole});
    }
}

void RadioBrowserModel::setShowToolTips(const bool showToolTips)
{
    if(std::exchange(m_showToolTips, showToolTips) == showToolTips) {
        return;
    }

    if(rowCount({}) > 0) {
        Q_EMIT dataChanged(index(0, 0), index(rowCount({}) - 1, columnCount({}) - 1), {Qt::ToolTipRole});
    }
}

void RadioBrowserModel::setShowSavedIndicators(const bool showSavedIndicators)
{
    if(std::exchange(m_showSavedIndicators, showSavedIndicators) == showSavedIndicators) {
        return;
    }

    if(rowCount({}) > 0) {
        Q_EMIT dataChanged(index(0, 0), index(rowCount({}) - 1, columnCount({}) - 1),
                           {SavedStationRole, Qt::ToolTipRole});
    }
}

void RadioBrowserModel::setSavedStations(const RadioStationList& stations)
{
    m_savedStations = stations;

    if(rowCount({}) > 0) {
        Q_EMIT dataChanged(index(0, 0), index(rowCount({}) - 1, columnCount({}) - 1),
                           {SavedStationRole, Qt::ToolTipRole});
    }
}

void RadioBrowserModel::setReorderEnabled(const bool enabled)
{
    if(m_reorderEnabled == enabled) {
        return;
    }

    m_reorderEnabled = enabled;
    if(m_reorderEnabled && m_sortColumn >= 0) {
        m_sortColumn = -1;
    }
}

void RadioBrowserModel::setApiSortingEnabled(const bool enabled)
{
    if(std::exchange(m_apiSortingEnabled, enabled) == enabled) {
        return;
    }

    if(m_apiSortingEnabled) {
        m_sortColumn = -1;
    }
}

void RadioBrowserModel::clearSort()
{
    m_sortColumn = -1;
}

void RadioBrowserModel::setCurrentStation(const RadioStation& station)
{
    const QString currentStreamUrl = m_currentStation.effectiveStreamUrl();
    const QString stationStreamUrl = station.effectiveStreamUrl();
    if(m_currentStation.uuid == station.uuid && currentStreamUrl == stationStreamUrl) {
        return;
    }

    const RadioStation previousStation = std::exchange(m_currentStation, station);

    for(int row{0}; std::cmp_less(row, m_stations.size()); ++row) {
        const RadioStation& rowStation = m_stations.at(row);
        if(isCurrentStation(rowStation) || previousStation.sameCurrentStationIdentity(rowStation)) {
            Q_EMIT dataChanged(index(row, 0), index(row, columnCount({}) - 1),
                               {Qt::BackgroundRole, IconBackgroundRole});
        }
    }
}

void RadioBrowserModel::updateColours()
{
    m_playingColour     = columnPlayingRowColor();
    m_iconPlayingColour = playingRowColor();

    if(rowCount({}) > 0) {
        Q_EMIT dataChanged(index(0, 0), index(rowCount({}) - 1, columnCount({}) - 1),
                           {Qt::BackgroundRole, IconBackgroundRole});
    }
}

bool RadioBrowserModel::reorderEnabled() const
{
    return m_reorderEnabled;
}

RadioStation RadioBrowserModel::stationAt(const int row) const
{
    if(row < 0 || std::cmp_greater_equal(row, m_stations.size())) {
        return {};
    }

    return m_stations.at(row);
}

void RadioBrowserModel::requestIcons(const QModelIndexList& indexes)
{
    if(!m_iconProvider) {
        return;
    }

    QSet<int> requestedRows;
    for(const QModelIndex& index : indexes) {
        const int row = index.row();
        if(!index.isValid() || requestedRows.contains(row) || row < 0
           || std::cmp_greater_equal(row, m_stations.size())) {
            continue;
        }

        requestedRows.insert(row);
        m_iconProvider->requestIcon(m_stations.at(row), iconBucketSize());
    }
}

void RadioBrowserModel::refreshIcons()
{
    m_placeholderIcons.clear();
    if(rowCount({}) > 0) {
        Q_EMIT dataChanged(index(0, 0), index(rowCount({}) - 1, columnCount({}) - 1), {Qt::DecorationRole});
    }
}

bool RadioBrowserModel::isCurrentStation(const RadioStation& station) const
{
    return m_currentStation.sameCurrentStationIdentity(station);
}

int RadioBrowserModel::iconBucketSize() const
{
    return radioIconBucketSize(m_iconSize);
}

QIcon RadioBrowserModel::stationIcon(const RadioStation& station) const
{
    if(m_iconProvider) {
        const QIcon icon = m_iconProvider->icon(station, iconBucketSize());
        if(!icon.isNull()) {
            return icon;
        }
    }
    return placeholderIcon(station);
}

QIcon RadioBrowserModel::placeholderIcon(const RadioStation& station) const
{
    const QString cacheKey
        = u"%1#%2"_s.arg(station.uuid.isEmpty() ? station.name : station.uuid, QString::number(iconBucketSize()));
    if(auto* icon = m_placeholderIcons.object(cacheKey)) {
        return *icon;
    }

    auto* icon = new QIcon{Utils::placeholderIcon(station, iconBucketSize())};
    m_placeholderIcons.insert(cacheKey, icon);
    return *icon;
}

bool RadioBrowserModel::isSavedStation(const RadioStation& station) const
{
    const QString key = station.stationKey();
    if(key.isEmpty()) {
        return false;
    }

    return std::ranges::any_of(m_savedStations,
                               [&key](const RadioStation& savedStation) { return savedStation.stationKey() == key; });
}

void RadioBrowserModel::sort(int column, const Qt::SortOrder order)
{
    if(m_reorderEnabled) {
        return;
    }

    if(column < 0 || column >= columnCount({})) {
        m_sortColumn = -1;
        return;
    }

    if(m_apiSortingEnabled) {
        Q_EMIT sortRequested(column, order);
        return;
    }

    m_sortColumn = column;
    m_sortOrder  = order;

    beginResetModel();
    sortStations();
    endResetModel();
}

void RadioBrowserModel::sortStations()
{
    if(m_sortColumn < 0) {
        return;
    }

    StringCollator collator;

    std::ranges::stable_sort(m_stations, [this, &collator](const RadioStation& lhs, const RadioStation& rhs) {
        int compare{0};

        switch(m_sortColumn) {
            case Station:
                compare = collator.compare(lhs.name, rhs.name);
                break;
            case Country:
                compare = collator.compare(displayNameForCountry(lhs.country, lhs.countryCode),
                                           displayNameForCountry(rhs.country, rhs.countryCode));
                break;
            case Language:
                compare = collator.compare(lhs.language, rhs.language);
                break;
            case Codec:
                compare = collator.compare(lhs.codec, rhs.codec);
                break;
            case Bitrate:
                compare = lhs.bitrate < rhs.bitrate ? -1 : (lhs.bitrate > rhs.bitrate ? 1 : 0);
                break;
            case Votes:
                compare = lhs.voteCount < rhs.voteCount ? -1 : (lhs.voteCount > rhs.voteCount ? 1 : 0);
                break;
            case Clicks:
                compare = lhs.clickCount < rhs.clickCount ? -1 : (lhs.clickCount > rhs.clickCount ? 1 : 0);
                break;
            case Tags:
                compare = collator.compare(lhs.tags, rhs.tags);
                break;
            default:
                compare = collator.compare(lhs.name, rhs.name);
                break;
        }

        if(compare == 0) {
            compare = collator.compare(lhs.name, rhs.name);
        }

        return m_sortOrder == Qt::AscendingOrder ? compare < 0 : compare > 0;
    });
}

void RadioBrowserModel::updateIconCaptionLineCount()
{
    int lineCount{1};

    const auto countLines = [this](const RadioStation& station) {
        int count{0};

        const auto countColumn = [&count, &station](const int column) {
            if(!stationColumnText(station, column).trimmed().isEmpty()) {
                ++count;
            }
        };

        if(m_iconColumnOrder.empty()) {
            countColumn(Station);
        }
        else {
            for(const int column : m_iconColumnOrder) {
                countColumn(column);
            }
        }

        return std::max(count, 1);
    };

    for(const RadioStation& station : m_stations) {
        lineCount = std::max(lineCount, countLines(station));
    }

    m_iconCaptionLineCount = lineCount;
}

void RadioBrowserModel::handleIconLoaded(const QString& favicon)
{
    if(favicon.isEmpty()) {
        return;
    }

    for(int row{0}; std::cmp_less(row, m_stations.size()); ++row) {
        if(m_stations.at(row).favicon.trimmed() == favicon) {
            Q_EMIT dataChanged(index(row, Station), index(row, Station), {Qt::DecorationRole});
        }
    }
}
} // namespace Fooyin::RadioBrowser
