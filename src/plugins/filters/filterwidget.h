/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include <core/trackfwd.h>
#include <gui/fywidget.h>

namespace Fooyin {
class SettingsManager;
class AutoHeaderView;

namespace Filters {
struct LibraryFilter;

class FilterWidget : public FyWidget
{
    Q_OBJECT

public:
    explicit FilterWidget(SettingsManager* settings, QWidget* parent = nullptr);
    ~FilterWidget() override;

    void changeFilter(const LibraryFilter& filter);
    void reset(const TrackList& tracks);

    void setScrollbarEnabled(bool enabled);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;
    void finalise() override;

    void tracksAdded(const TrackList& tracks);
    void tracksUpdated(const TrackList& tracks);
    void tracksRemoved(const TrackList& tracks);

signals:
    void doubleClicked(const QString& playlistName);
    void middleClicked(const QString& playlistName);

    void selectionChanged(const LibraryFilter& filter, const QString& playlistName);
    void filterDeleted(const LibraryFilter& filter);
    void requestColumnsChange(const LibraryFilter& filter, const ColumnIds& columns);
    void requestHeaderMenu(const LibraryFilter& filter, AutoHeaderView* header, const QPoint& pos);
    void requestContextMenu(const LibraryFilter& filter, const QPoint& pos);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Filters
} // namespace Fooyin
