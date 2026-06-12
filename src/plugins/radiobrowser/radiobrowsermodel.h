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

#pragma once

#include "radiostation.h"

#include <QAbstractTableModel>
#include <QCache>
#include <QIcon>
#include <QSize>

#include <vector>

class QMimeData;

namespace Fooyin::RadioBrowser {
class RadioIconProvider;

class RadioBrowserModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Role : uint16_t
    {
        NameRole = Qt::UserRole,
        CountryRole,
        LanguageRole,
        CodecRole,
        BitrateRole,
        ClickCountRole,
        VoteCountRole,
        TagsRole,
        FaviconRole,
        IconCaptionLinesRole,
        IconCaptionLineCountRole,
        IconBackgroundRole,
        SavedStationRole,
    };

    explicit RadioBrowserModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    [[nodiscard]] QStringList mimeTypes() const override;
    [[nodiscard]] QMimeData* mimeData(const QModelIndexList& indexes) const override;
    [[nodiscard]] bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                                    const QModelIndex& parent) override;
    [[nodiscard]] Qt::DropActions supportedDropActions() const override;
    [[nodiscard]] Qt::DropActions supportedDragActions() const override;
    void sort(int column, Qt::SortOrder order) override;

    void setIconProvider(RadioIconProvider* provider);
    void setStations(const RadioStationList& stations);
    void appendStations(const RadioStationList& stations);
    void setIconColumnOrder(const std::vector<int>& order);
    void setIconSize(const QSize& size);
    void setRowHeight(int height);
    void setShowIcons(bool showIcons);
    void setShowToolTips(bool showToolTips);
    void setShowSavedIndicators(bool showSavedIndicators);
    void setSavedStations(const RadioStationList& stations);
    void setReorderEnabled(bool enabled);
    void setApiSortingEnabled(bool enabled);
    void clearSort();
    void setCurrentStation(const RadioStation& station);
    void updateColours();
    [[nodiscard]] bool reorderEnabled() const;
    [[nodiscard]] RadioStation stationAt(int row) const;
    void requestIcons(const QModelIndexList& indexes);
    void refreshIcons();

Q_SIGNALS:
    void sortRequested(int column, Qt::SortOrder order);
    void stationsReordered(const Fooyin::RadioBrowser::RadioStationList& stations);

private:
    void sortStations();
    void updateIconCaptionLineCount();
    [[nodiscard]] bool isCurrentStation(const RadioStation& station) const;
    [[nodiscard]] int iconBucketSize() const;
    [[nodiscard]] QIcon stationIcon(const RadioStation& station) const;
    [[nodiscard]] QIcon placeholderIcon(const RadioStation& station) const;
    [[nodiscard]] bool isSavedStation(const RadioStation& station) const;
    void handleIconLoaded(const QString& favicon);

    RadioStationList m_stations;
    RadioStationList m_savedStations;
    RadioStation m_currentStation;
    QColor m_playingColour;
    QColor m_iconPlayingColour;
    RadioIconProvider* m_iconProvider;
    mutable QCache<QString, QIcon> m_placeholderIcons;
    std::vector<int> m_iconColumnOrder;
    QSize m_iconSize;
    int m_iconCaptionLineCount;
    int m_rowHeight;
    int m_sortColumn;
    Qt::SortOrder m_sortOrder;
    bool m_reorderEnabled;
    bool m_apiSortingEnabled;
    bool m_showIcons;
    bool m_showToolTips;
    bool m_showSavedIndicators;
};
} // namespace Fooyin::RadioBrowser
