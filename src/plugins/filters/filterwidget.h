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

#include <core/track.h>
#include <gui/fywidget.h>

namespace Fooyin {
class AutoHeaderView;
class CoverProvider;
class SettingsManager;
class WidgetContext;

namespace Filters {
class FilterColumnRegistry;

class FilterWidget : public FyWidget
{
    Q_OBJECT

public:
    explicit FilterWidget(FilterColumnRegistry* columnRegistry, CoverProvider* coverProvider, SettingsManager* settings,
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
    void tracksUpdated(const TrackList& tracks);
    void tracksPlayed(const TrackList& tracks);
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
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Filters
} // namespace Fooyin
