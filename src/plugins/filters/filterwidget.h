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

#include "filterfwd.h"

#include <core/track.h>
#include <gui/fywidget.h>
#include <utils/widgets/expandedtreeview.h>

namespace Fooyin {
class AutoHeaderView;
class CoverProvider;
class LibraryManager;
class SettingsManager;
class WidgetContext;

namespace Filters {
class FilterColumnRegistry;
class FilterModel;
class FilterSortModel;
class FilterWidgetPrivate;

class FilterWidget : public FyWidget
{
    Q_OBJECT

public:
    explicit FilterWidget(FilterColumnRegistry* columnRegistry, LibraryManager* libraryManager,
                          CoverProvider* coverProvider, SettingsManager* settings, QWidget* parent = nullptr);
    ~FilterWidget() override;

    [[nodiscard]] Id group() const;
    [[nodiscard]] int index() const;
    [[nodiscard]] bool multipleColumns() const;
    [[nodiscard]] bool isActive() const;
    [[nodiscard]] TrackList tracks() const;
    [[nodiscard]] TrackList filteredTracks() const;
    [[nodiscard]] QString searchFilter() const;
    [[nodiscard]] WidgetContext* widgetContext() const;

    void setGroup(const Id& group);
    void setIndex(int index);
    void refetchFilteredTracks();
    void setFilteredTracks(const TrackList& tracks);
    void clearFilteredTracks();

    void reset(const TrackList& tracks);
    void softReset(const TrackList& tracks);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;
    void finalise() override;

    void searchEvent(const QString& search) override;

    void tracksAdded(const TrackList& tracks);
    void tracksChanged(const TrackList& tracks);
    void tracksUpdated(const TrackList& tracks);
    void tracksRemoved(const TrackList& tracks);

signals:
    void doubleClicked();
    void middleClicked();

    void filterDeleted();
    void filterUpdated();
    void finishedUpdating();
    void selectionChanged();
    void requestHeaderMenu(Fooyin::AutoHeaderView* header, const QPoint& pos);
    void requestContextMenu(const QPoint& pos);
    void requestEditConnections();
    void requestSearch(const QString& search);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void setupConnections();

    void refreshFilteredTracks();
    void handleSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
    void updateViewMode(ExpandedTreeView::ViewMode mode);
    void updateCaptions(ExpandedTreeView::CaptionDisplay captions);
    void hideHeader(bool hide);
    void setScrollbarEnabled(bool enabled);

    void addDisplayMenu(QMenu* menu);
    void filterHeaderMenu(const QPoint& pos);

    [[nodiscard]] bool hasColumn(int id) const;
    void columnChanged(const FilterColumn& changedColumn);
    void columnRemoved(int id);

    FilterColumnRegistry* m_columnRegistry;
    SettingsManager* m_settings;

    ExpandedTreeView* m_view;
    AutoHeaderView* m_header;
    FilterModel* m_model;
    FilterSortModel* m_sortProxy;

    Id m_group;
    int m_index{-1};
    FilterColumnList m_columns;
    bool m_multipleColumns{false};
    TrackList m_tracks;
    TrackList m_filteredTracks;

    WidgetContext* m_widgetContext;

    QString m_searchStr;
    bool m_searching{false};
    bool m_updating{false};

    QByteArray m_headerState;
};
} // namespace Filters
} // namespace Fooyin
