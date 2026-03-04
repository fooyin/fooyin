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
#include <gui/widgets/expandedtreeview.h>

namespace Fooyin {
class AutoHeaderView;
class ActionManager;
class CoverProvider;
class LibraryManager;
class SettingsManager;
class SignalThrottler;
class WidgetContext;
enum class TrackAction;

namespace Filters {
class FilterColumnRegistry;
class FilterModel;
class FilterSortModel;

class FilterWidget : public FyWidget
{
    Q_OBJECT

public:
    explicit FilterWidget(ActionManager* actionManager, FilterColumnRegistry* columnRegistry,
                          LibraryManager* libraryManager, CoverProvider* coverProvider, SettingsManager* settings,
                          QWidget* parent = nullptr);
    ~FilterWidget() override;

    [[nodiscard]] Id group() const;
    [[nodiscard]] int index() const;
    [[nodiscard]] bool multipleColumns() const;
    [[nodiscard]] bool isActive() const;
    [[nodiscard]] TrackList tracks() const;
    [[nodiscard]] TrackList filteredTracks() const;
    [[nodiscard]] QString searchFilter() const;
    [[nodiscard]] WidgetContext* widgetContext() const;
    [[nodiscard]] TrackAction doubleClickAction() const;
    [[nodiscard]] TrackAction middleClickAction() const;
    [[nodiscard]] bool sendPlayback() const;
    [[nodiscard]] bool playlistEnabled() const;
    [[nodiscard]] bool autoSwitch() const;
    [[nodiscard]] bool keepAlive() const;
    [[nodiscard]] QString playlistName() const;
    [[nodiscard]] bool hasSelection() const;
    void openConfigDialog() override;

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

    struct ConfigData
    {
        int doubleClickAction{1};
        int middleClickAction{0};
        bool sendPlayback{true};
        bool playlistEnabled{true};
        bool autoSwitch{true};
        bool keepAlive{false};
        QString playlistName;
        int rowHeight{0};
        QSize iconSize{100, 100};
    };

    [[nodiscard]] ConfigData defaultConfig() const;
    [[nodiscard]] const ConfigData& currentConfig() const;
    void saveDefaults(const ConfigData& config) const;
    void applyConfig(const ConfigData& config);

    void tracksAdded(const TrackList& tracks);
    void tracksChanged(const TrackList& tracks);
    void tracksUpdated(const TrackList& tracks);
    void tracksRemoved(const TrackList& tracks);

    void addFilterHeaderMenu(QMenu* menu, const QPoint& pos);

signals:
    void doubleClicked();
    void middleClicked();

    void filterDeleted();
    void filterUpdated();
    void finishedUpdating();
    void selectionChanged();
    void configChanged();
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
    void updateAppearance();

    void addDisplayMenu(QMenu* menu);
    void filterHeaderMenu(const QPoint& pos);

    [[nodiscard]] bool hasColumn(int id) const;
    void columnChanged(const FilterColumn& changedColumn);
    void columnRemoved(int id);

    [[nodiscard]] ConfigData configFromLayout(const QJsonObject& layout) const;
    static void saveConfigToLayout(const ConfigData& config, QJsonObject& layout);

    ActionManager* m_actionManager;
    FilterColumnRegistry* m_columnRegistry;
    SettingsManager* m_settings;

    ExpandedTreeView* m_view;
    AutoHeaderView* m_header;
    FilterModel* m_model;
    FilterSortModel* m_sortProxy;
    SignalThrottler* m_resetThrottler;

    Id m_group;
    int m_index;
    FilterColumnList m_columns;
    bool m_multipleColumns;
    TrackList m_tracks;
    TrackList m_filteredTracks;

    WidgetContext* m_widgetContext;

    QString m_searchStr;
    bool m_searching;
    bool m_updating;

    QByteArray m_headerState;

    bool m_showHeader;
    bool m_showScrollbar;
    bool m_alternatingColours;
    ConfigData m_config;
};
} // namespace Filters
} // namespace Fooyin
